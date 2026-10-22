#include "Cli/MatchRequest.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
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
    if (token(input) != "CORGiReg1") throw std::invalid_argument("Unsupported matching protocol");
    auto graph = std::make_shared<ImportedGraph>();
    const auto nodeCount = number(input);
    for (std::uint64_t i = 0; i < nodeCount; ++i) {
        const auto id = number(input);
        auto key = unhex(input);
        if (key.empty() || key.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789") != std::string::npos)
            throw std::invalid_argument("Invalid node predicate");
        graph->addNode(id, std::move(key), unhex(input));
    }
    const auto edgeCount = number(input);
    for (std::uint64_t i = 0; i < edgeCount; ++i) {
        const auto from = number(input);
        const auto to = number(input);
        graph->addEdge(from, to);
    }
    const auto query = unhex(input);
    if (query.empty()) throw std::invalid_argument("Empty regex");
    const auto budget = number(input);
    if (budget == 0) throw std::invalid_argument("Search budget must be positive");
    std::string extra;
    if (input >> extra) throw std::invalid_argument("Unexpected request data");
    const auto pattern = PGInterpreterTG(query).interpret();
    NodePredicates predicates;
    for (const auto& entry : graph->getNodes()) {
        const auto key = entry.second.predicate;
        predicates.emplace(key, [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; });
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
    output << "]}\n";
}
}
