#include "Cli/MatchRequest.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "NodeTest/NodeRule.hpp"
#include <cmath>
#include <locale>
#include <sstream>
#include <istream>
#include <ostream>

namespace CorGIReg {
namespace {
std::string token(std::istream& input) {
    std::string value;
    if (!(input >> value)) throw std::invalid_argument("Incomplete matching request");
    return value;
}
std::uint64_t number(std::istream& input) {
    const auto value = token(input);
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("Invalid unsigned integer in matching request");
    std::size_t used;
    const auto result = std::stoull(value, &used);
    if (used != value.size()) throw std::invalid_argument("Invalid integer");
    return result;
}
std::string unhex(std::istream& input) {
    auto value = token(input);
    if (value == "-") return {};
    if (value.size() % 2 != 0) throw std::invalid_argument("Invalid hexadecimal string");
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::invalid_argument("Invalid hexadecimal string");
    };
    std::string result;
    for (std::size_t i = 0; i < value.size(); i += 2)
        result += static_cast<char>((digit(value[i]) << 4) | digit(value[i + 1]));
    return result;
}
void jsonString(std::ostream& out, const std::string& text) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << c;
    }
    out << '"';
}
NodeValue readValue(std::istream& input, unsigned depth = 0) {
    if (depth > 64) throw std::invalid_argument("Node document nesting exceeds 64");
    const auto type = token(input);
    if (type == "n") return nullptr;
    if (type == "m") return NodeValue::Missing{};
    if (type == "b") {
        auto value = number(input);
        if (value > 1) throw std::invalid_argument("Invalid boolean value");
        return value != 0;
    }
    if (type == "u") return NodeValue(number(input));
    if (type == "i") {
        auto value = token(input);
        auto digits = value.substr(!value.empty() && value[0] == '-' ? 1 : 0);
        if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("Invalid signed value");
        return NodeValue(std::int64_t(std::stoll(value)));
    }
    if (type == "d") {
        std::istringstream stream(token(input));
        stream.imbue(std::locale::classic());
        double value;
        if (!(stream >> value) || !stream.eof() || !std::isfinite(value))
            throw std::invalid_argument("Invalid finite numeric value");
        return value;
    }
    if (type == "s") return unhex(input);
    if (type == "a" || type == "o") {
        const auto count = number(input);
        if (count > 1000000) throw std::invalid_argument("Node value collection exceeds 1000000 items");
        if (type == "a") {
            NodeValue::Array result;
            for (std::uint64_t i = 0; i < count; ++i) result.push_back(readValue(input, depth + 1));
            return result;
        }
        NodeValue::Object result;
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto key = unhex(input);
            auto value = readValue(input, depth + 1);
            if (!result.emplace(key, std::move(value)).second) throw std::invalid_argument("Duplicate node document key");
        }
        return result;
    }
    throw std::invalid_argument("Unknown node value type");
}
template<class Range> void numbers(std::ostream& out, const Range& values) {
    out << '[';
    bool first = true;
    for (auto value : values) { if (!first) out << ','; first = false; out << value; }
    out << ']';
}
void writeMatch(std::ostream& out, const MatchSolution& match) {
    out << "{\"node_ids\":";
    numbers(out, match.getAll());
    out << ",\"starts\":[";
    bool first = true;
    for (auto start : match.getStartNode()) {
        if (!first) out << ',';
        first = false;
        if (start) out << *start; else out << "null";
    }
    out << "],\"walks\":[";
    first = true;
    for (const auto& walk : match.getWalks()) {
        if (!first) out << ',';
        first = false;
        out << "{\"origins\":"; numbers(out, walk.origins);
        out << ",\"nodes\":[";
        bool firstNode = true;
        for (const auto& node : walk.nodes) {
            if (!firstNode) out << ',';
            firstNode = false;
            out << "{\"id\":" << node.node << ",\"name\":";
            jsonString(out, match.getNodeName(node.node));
            out << ",\"predicate\":"; jsonString(out, node.predicate); out << '}';
        }
        out << "],\"quantifiers\":[";
        bool firstQ = true;
        for (const auto& q : walk.quantifiers) {
            if (!firstQ) out << ',';
            firstQ = false;
            out << "{\"index\":" << q.index << ",\"symbol\":";
            jsonString(out, std::string(1, q.symbol));
            out << ",\"count\":" << q.count() << ",\"parent\":";
            if (q.parent) out << *q.parent; else out << "null";
            out << ",\"begin\":" << q.begin << ",\"end\":" << q.end << ",\"iteration_ends\":";
            numbers(out, q.iterationEnds); out << '}';
        }
        out << "]}";
    }
    out << "]}";
}
}

void matchRequest(std::istream& input, std::ostream& output) {
    const auto protocol = token(input);
    if (protocol != "CORGiReg1" && protocol != "CORGiReg2" && protocol != "CORGiReg3")
        throw std::invalid_argument("Unsupported matching protocol");
    auto graph = std::make_shared<ImportedGraph>();
    std::map<NodeId, NodeValue> documents;
    const auto nodeCount = number(input);
    for (std::uint64_t i = 0; i < nodeCount; ++i) {
        const auto id = number(input);
        auto key = unhex(input);
        if (key.empty() || key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789") != std::string::npos)
            throw std::invalid_argument("Invalid node predicate");
        graph->addNode(id, std::move(key), unhex(input));
        if (protocol == "CORGiReg3") documents.emplace(id, readValue(input));
    }
    const auto edgeCount = number(input);
    for (std::uint64_t i = 0; i < edgeCount; ++i) {
        const auto from = number(input);
        const auto to = number(input);
        graph->addEdge(from, to);
    }
    // Definitions are per request, never added to the legacy global dictionary.
    std::map<std::string, std::optional<NodeIds>> definitions;
    if (protocol != "CORGiReg1") {
        const auto count = number(input);
        for (std::uint64_t i = 0; i < count; ++i) {
            const auto key = unhex(input);
            if (key.empty() || key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789") != std::string::npos)
                throw std::invalid_argument("Invalid library predicate name");
            const auto available = number(input);
            if (available > 1) throw std::invalid_argument("Invalid definition availability flag");
            std::optional<NodeIds> ids;
            if (available) {
                ids.emplace();
                const auto size = number(input);
                for (std::uint64_t j = 0; j < size; ++j) {
                    const auto id = number(input);
                    graph->getNodes().at(id);
                    ids->insert(id);
                }
                if (protocol == "CORGiReg3") {
                    auto rule = readValue(input);
                    const auto source = unhex(input);
                    if (!std::holds_alternative<std::nullptr_t>(rule.data)) {
                        NodeRule check;
                        try { check = compileNodeRule(rule); }
                        catch (const std::invalid_argument& error) {
                            throw std::invalid_argument(source + " (" + key + "): " + error.what());
                        }
                        for (auto it = ids->begin(); it != ids->end();) {
                            if (check(documents.at(*it))) ++it;
                            else it = ids->erase(it);
                        }
                    }
                }
            }
            if (!definitions.emplace(key, std::move(ids)).second)
                throw std::invalid_argument("Duplicate library predicate: " + key);
        }
    }
    const auto query = unhex(input);
    if (query.empty()) throw std::invalid_argument("Empty regex");
    const auto budget = number(input);
    if (budget == 0) throw std::invalid_argument("Search budget must be positive");
    std::string extra;
    if (input >> extra) throw std::invalid_argument("Unexpected request data");
    const auto pattern = PGInterpreterTG(query).interpret();
    for (const auto& transition : pattern->getTransitions()) {
        const auto found = definitions.find(transition->getTestKey());
        if (found != definitions.end() && !found->second)
            throw std::invalid_argument("No definition for predicate '" + found->first + "' in the selected input format");
    }
    NodePredicates predicates;
    for (const auto& entry : graph->getNodes()) {
        const auto key = entry.second.predicate;
        predicates.emplace(key, [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; });
    }
    for (const auto& definition : definitions) {
        if (definition.second)
            predicates[definition.first] = [ids = *definition.second](NodeId id) { return ids.count(id) != 0; };
    }
    std::vector<std::optional<NodeId>> candidates;
    for (const auto& entry : graph->getNodes()) candidates.push_back(entry.first);
    if (candidates.empty()) candidates.push_back(std::nullopt);
    const auto arity = pattern->getNbStart();
    std::uint64_t combinations = 1;
    for (std::size_t i = 0; i < arity; ++i) {
        if (combinations > budget / candidates.size())
            throw std::invalid_argument("Too many starting combinations; increase --max-starts or use a smaller graph");
        combinations *= candidates.size();
    }
    output << "{\"query\":"; jsonString(output, query); output << ",\"matches\":[";
    bool first = true;
    for (std::uint64_t combination = 0; combination < combinations; ++combination) {
        auto rest = combination;
        std::vector<std::optional<NodeId>> starts;
        for (std::size_t i = 0; i < arity; ++i) {
            starts.push_back(candidates[rest % candidates.size()]); rest /= candidates.size();
        }
        const auto result = pattern->test(graph, starts, predicates);
        for (const auto& match : result->getMatches()) {
            if (!first) output << ',';
            first = false;
            writeMatch(output, *match);
        }
    }
    output << ']';
    if (protocol == "CORGiReg3") {
        output << ",\"definition_nodes\":{";
        bool firstDefinition = true;
        for (const auto& definition : definitions) {
            if (!firstDefinition) output << ',';
            firstDefinition = false;
            jsonString(output, definition.first); output << ':';
            if (definition.second) numbers(output, *definition.second); else output << "null";
        }
        output << '}';
    }
    output << "}\n";
}
}
