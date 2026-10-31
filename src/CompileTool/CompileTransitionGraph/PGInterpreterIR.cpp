#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include <fstream>
#include <limits>


namespace CorGIReg{

IRGenerationOptions IRGenerationOptions::read(std::istream& input) {
    IRGenerationOptions result;
    std::set<std::string> seen;
    auto trim = [](const std::string& text) {
        const auto begin = text.find_first_not_of(" \t\r\n");
        return begin == std::string::npos ? std::string{} :
            text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    };
    auto number = [](const std::string& value, unsigned long long maximum) {
        if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("Expected an unsigned integer");
        const auto parsed = std::stoull(value);
        if (parsed > maximum) throw std::invalid_argument("Integer exceeds allowed limit");
        return parsed;
    };
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;
        try {
            const auto separator = line.find('=');
            if (separator == std::string::npos) throw std::invalid_argument("Expected key = value");
            const auto key = trim(line.substr(0, separator));
            const auto value = trim(line.substr(separator + 1));
            if (!seen.insert(key).second) throw std::invalid_argument("Duplicate key");
            if (key == "mode") {
                if (value != "random" && value != "deterministic") throw std::invalid_argument("Unknown mode");
                result.randomMissing = value == "random";
            } else if (key == "seed") {
                result.seed = static_cast<std::uint32_t>(number(value, std::numeric_limits<std::uint32_t>::max()));
            } else if (key == "max_loops") {
                result.maxLoops = static_cast<std::size_t>(number(value, 1000000));
            } else if (key == "max_nodes") {
                result.maxNodes = static_cast<std::size_t>(number(value, std::numeric_limits<std::size_t>::max()));
            } else {
                const auto id = static_cast<std::size_t>(number(key, 1000000));
                if (!id || result.repetitions.count(id)) throw std::invalid_argument("Invalid or duplicate quantifier index");
                auto& values = result.repetitions[id];
                std::size_t begin = 0;
                do {
                    const auto end = value.find(',', begin);
                    values.push_back(static_cast<std::size_t>(number(trim(value.substr(begin, end - begin)), 1000000)));
                    if (end == std::string::npos) break;
                    begin = end + 1;
                } while (true);
            }
        } catch (const std::exception& error) {
            throw std::invalid_argument("IR values line " + std::to_string(lineNumber) + ": " + error.what());
        }
    }
    if (input.bad() || (input.fail() && !input.eof())) throw std::runtime_error("Cannot read IR values");
    return result;
}

IRGenerationOptions IRGenerationOptions::fromFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open IR values: " + path);
    return read(input);
}

PGInterpreterIR::PGInterpreterIR(const std::string& expression, const std::string& valuesFile)
    : PGInterpreterIR(expression, IRGenerationOptions::fromFile(valuesFile)) {}

namespace {
using IRTree = std::shared_ptr<AstNode<tGTokensTypes>>;

// Separate expansion from parsing: each visit creates a fresh occurrence, so
// nested repetitions can consume different values without copying a template.
class ConfiguredIRGenerator {
    struct Fragment {
        std::vector<NodeNN::Ptr> first, last;
        bool common = false;
        bool boundary = false; // Ends with '$': nothing may follow `last`.
    };
    const IRGenerationOptions& options;
    std::mt19937 random;
    std::map<const AstNode<tGTokensTypes>*, std::size_t> indices;
    std::map<std::size_t, tGTokensTypes> kinds;
    std::map<std::size_t, std::size_t> consumed;
    std::map<std::string, NodeNN::Ptr> anchors;
    std::shared_ptr<GraphNN> graph = std::make_shared<GraphNN>();
    std::size_t visits = 0;
    std::map<NodeNN::Ptr, PortNumber> nextInput;
    std::map<NodeNN::Ptr, bool> automaticInputs;

    void connect(const NodeNN::Ptr& from, const NodeNN::Ptr& to, const std::string& port) {
        const bool automatic = port.empty() || port == "*";
        auto mode = automaticInputs.emplace(to, automatic);
        if (!mode.second && mode.first->second != automatic)
            throw std::invalid_argument("Cannot mix automatic and fixed input ports on one node");
        PortNumber input = automatic ? ++nextInput[to] : static_cast<PortNumber>(std::stoull(port));
        for (const auto& parent : to->getParents()) for (const auto& binding : parent->getConnectionPorts(to))
            if (binding.input == input && parent != from)
                throw std::invalid_argument("Input port has multiple producers");
        graph->addEdge(from, to, {input, std::nullopt});
    }

    void index(const IRTree& tree) {
        for (const auto& child : tree->getChilds()) index(child);
        auto type = tree->getType();
        if ((type == tGTokensTypes::KEY || type == tGTokensTypes::CKEY || type == tGTokensTypes::SCKEY) &&
            !parseNodePattern(tree->getValue()).conditions.empty())
            throw std::invalid_argument("Inline node conditions are supported for matching, not graph generation");
        if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO) {
            auto id = indices.size() + 1;
            indices.emplace(tree.get(), id);
            kinds.emplace(id, type);
        }
    }
    std::size_t count(const IRTree& tree) {
        const auto id = indices.at(tree.get());
        const auto invocation = consumed[id]++;
        const auto found = options.repetitions.find(id);
        if (found != options.repetitions.end()) {
            const auto& values = found->second;
            if (values.size() == 1) return values.front();
            if (invocation < values.size()) return values[invocation];
            throw std::invalid_argument("Exhausted values for quantifier " + std::to_string(id));
        }
        if (!options.randomMissing)
            throw std::invalid_argument("Missing value for quantifier " + std::to_string(id));
        const auto type = tree->getType();
        return std::uniform_int_distribution<std::size_t>(type == tGTokensTypes::QOM ? 1 : 0,
            type == tGTokensTypes::QZO ? 1 : options.maxLoops + (type == tGTokensTypes::QOM ? 1 : 0))(random);
    }
    static void anchored(const Fragment& fragment) {
        if (fragment.boundary && fragment.first.empty())
            throw std::invalid_argument("Unsatisfiable boundary: '$' has no preceding operation");
    }
    Fragment join(Fragment left, Fragment right, bool reverse, bool separate = false, const std::string& port = "") {
        if (reverse) std::swap(left, right);
        if (separate) { anchored(left); anchored(right); }
        if (!port.empty() && (left.first.empty() || right.first.empty()))
            throw std::invalid_argument("A numbered connection requires two nonempty operands");
        if (!separate && left.boundary && !right.first.empty())
            throw std::invalid_argument("Unsatisfiable boundary: '$' is followed by an operation");
        if (!separate && right.boundary && right.first.empty()) {
            // The boundary also applies through skipped operands: A->B?->$ with
            // no B makes A terminal, as in matching.
            if (left.first.empty()) return right;
            for (const auto& node : left.last) node->setInfo("isTerminal", true);
            left.boundary = true;
            left.common = left.common || right.common;
            return left;
        }
        if (left.first.empty()) return right;
        if (right.first.empty()) return left;
        if (separate) {
            left.first.insert(left.first.end(), right.first.begin(), right.first.end());
            left.last.insert(left.last.end(), right.last.begin(), right.last.end());
        } else {
            for (const auto& from : left.last) for (const auto& to : right.first)
                connect(from, to, port);
            left.last = std::move(right.last);
            left.boundary = right.boundary;
        }
        left.common = left.common || right.common;
        return left;
    }
    Fragment visit(const IRTree& tree) {
        // Also bounds empty nested expansions, which have no nodes to count.
        if (++visits > 1000000) throw std::length_error("IR expansion exceeds visit limit");
        const auto type = tree->getType();
        const auto& children = tree->getChilds();
        if (type == tGTokensTypes::NEXT || type == tGTokensTypes::PREC || type == tGTokensTypes::SEP) {
            auto left = visit(children.at(0));
            auto right = visit(children.at(1));
            return join(std::move(left), std::move(right), type == tGTokensTypes::PREC, type == tGTokensTypes::SEP, tree->getValue());
        }
        if (type == tGTokensTypes::LPAREN) return visit(children.at(0));
        if (type == tGTokensTypes::END) return {{}, {}, false, true};
        if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO) {
            const auto repetitions = count(tree);
            Fragment result;
            for (std::size_t i = 0; i < repetitions; ++i) {
                auto body = visit(children.at(0));
                if (!body.common || result.first.empty() || body.first.empty()) {
                    result = join(std::move(result), std::move(body), false);
                    continue;
                }
                // Iterations sharing an anchor are not chained. The group is entered
                // through its first iteration and left through its last one, as a
                // plain neighbor accepts a single parent and child.
                result.last = std::move(body.last);
                result.common = true;
                result.boundary = body.boundary;
            }
            return result;
        }
        const bool common = type == tGTokensTypes::CKEY || type == tGTokensTypes::SCKEY;
        if (type != tGTokensTypes::KEY && !common) throw std::logic_error("Unsupported IR node");
        std::string key = tree->getValue(), predicate = key;
        if (common) {
            auto parsed = parseCommonKey(key);
            key = parsed.identity(); predicate = parsed.predicate;
            auto found = anchors.find(key);
            if (found != anchors.end()) return {{found->second}, {found->second}, true};
        }
        if (graph->getNodes().size() >= options.maxNodes) throw std::length_error("IR exceeds maxNodes");
        auto node = std::make_shared<NodeNN>();
        node->setInfo("Type", predicate);
        node->setInfo("Key", key);
        if (common) { node->setInfo("isCommon", true); anchors.emplace(key, node); }
        graph->addNode(node);
        return {{node}, {node}, common};
    }
public:
    explicit ConfiguredIRGenerator(const IRGenerationOptions& value) : options(value), random(value.seed) {}
    std::shared_ptr<GraphNN> run(const IRTree& tree) {
        index(tree);
        if (options.maxLoops > 1000000) throw std::invalid_argument("maxLoops exceeds expansion limit");
        for (const auto& item : options.repetitions) {
            if (!kinds.count(item.first) || item.second.empty())
                throw std::invalid_argument("Unknown or empty quantifier " + std::to_string(item.first));
            for (auto value : item.second) {
                auto type = kinds.at(item.first);
                if (value > 1000000 || (type == tGTokensTypes::QOM && value == 0) ||
                    (type == tGTokensTypes::QZO && value > 1))
                    throw std::invalid_argument("Invalid value for quantifier " + std::to_string(item.first));
            }
        }
        anchored(visit(tree));
        for (const auto& node : graph->getNodes())
            if (node->hasArgument("isTerminal") && !node->getChildren().empty())
                throw std::invalid_argument("Unsatisfiable boundary: an operation before '$' has a successor");
        for (const auto& item : options.repetitions)
            if (item.second.size() > 1 && consumed[item.first] != item.second.size())
                throw std::invalid_argument("Unused values for quantifier " + std::to_string(item.first));
        return graph;
    }
};
}

PGInterpreterIR::PGInterpreterIR(const std::string& expression, IRGenerationOptions options)
    : mParser(expression), mOptions(std::move(options)) {}

PGInterpreterIR::PGInterpreterIR(const std::string expression)
    : PGInterpreterIR(expression, 10) {}

PGInterpreterIR::PGInterpreterIR(const std::string expression, size_t maxLoops)
    : mParser(expression) {
    mOptions.randomMissing = true;
    mOptions.maxLoops = maxLoops;
    mOptions.seed = std::random_device{}();
}

std::shared_ptr<GraphNN> PGInterpreterIR::interpret() {
    return ConfiguredIRGenerator(mOptions).run(mParser.parse());
}

} // namespace CorGIReg
