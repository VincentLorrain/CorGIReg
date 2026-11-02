#include <catch2/catch_test_macros.hpp>
#include "Cli/MatchRequest.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include <iomanip>
#include <sstream>

using namespace CorGIReg;

namespace {
using NodeSets = std::set<std::set<NodeId>>;

// Explicit operation graph shared by the interpreter and public tests.
//  0:X->1:A->2:B->3:Y   10:X->11:C->12:Y   20:X->21:A->22:Y   30:X->31:B->32:Y
//  40:X->41:A->42:C->43:A->44:Y   50:X->51:Y
//  60:X->61:A->62:B->63:Y, 60->64:Z   70:C->71:X->72:Z   80:B->81:X->82:Z   90:X->91:C
struct Fixture {
    std::vector<std::pair<NodeId, std::string>> nodes{
        {0, "X"}, {1, "A"}, {2, "B"}, {3, "Y"}, {10, "X"}, {11, "C"}, {12, "Y"}, {20, "X"}, {21, "A"}, {22, "Y"},
        {30, "X"}, {31, "B"}, {32, "Y"}, {40, "X"}, {41, "A"}, {42, "C"}, {43, "A"}, {44, "Y"}, {50, "X"}, {51, "Y"},
        {60, "X"}, {61, "A"}, {62, "B"}, {63, "Y"}, {64, "Z"}, {70, "C"}, {71, "X"}, {72, "Z"},
        {80, "B"}, {81, "X"}, {82, "Z"}, {90, "X"}, {91, "C"}};
    std::vector<std::pair<NodeId, NodeId>> edges{
        {0, 1}, {1, 2}, {2, 3}, {10, 11}, {11, 12}, {20, 21}, {21, 22}, {30, 31}, {31, 32},
        {40, 41}, {41, 42}, {42, 43}, {43, 44}, {50, 51}, {60, 61}, {61, 62}, {62, 63}, {60, 64},
        {70, 71}, {71, 72}, {80, 81}, {81, 82}, {90, 91}};
    std::shared_ptr<ImportedGraph> graph = std::make_shared<ImportedGraph>();
    NodePredicates predicates;

    Fixture() {
        for (const auto& [id, op] : nodes) graph->addNode(id, op, "n" + std::to_string(id));
        for (const auto& [from, to] : edges) graph->addEdge(from, to);
        for (const auto& [id, op] : nodes)
            predicates[op] = [graph = graph, key = op](NodeId node) { return graph->getNodes().at(node).predicate == key; };
    }
    // Interpreter level: one start node for a single-path pattern.
    NodeSets fromStart(const std::string& query, NodeId start) const {
        // Keep the solution alive while reading its matches.
        const auto solution = PGInterpreterTG(query).interpret()->test(graph, {start}, predicates);
        NodeSets result;
        for (const auto& match : solution->getMatches()) result.insert(match->getAll());
        return result;
    }
    // Public level: the native worker request searches every start, rooted.
    NodeSets search(const std::string& query) const {
        auto hex = [](const std::string& text) {
            std::ostringstream out;
            for (unsigned char c : text) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            return out.str();
        };
        std::ostringstream request;
        request << "CORGiReg1 " << nodes.size();
        for (const auto& [id, op] : nodes) request << ' ' << id << ' ' << hex(op) << " -";
        request << ' ' << edges.size();
        for (const auto& [from, to] : edges) request << ' ' << from << ' ' << to;
        request << ' ' << hex(query) << " 100000";
        std::istringstream input(request.str());
        std::ostringstream output;
        matchRequest(input, output);
        NodeSets result;
        const auto report = parseJson(output.str());
        for (const auto& match : std::get<NodeValue::Array>(std::get<NodeValue::Object>(report.data).at("matches").data)) {
            std::set<NodeId> ids;
            for (const auto& id : std::get<NodeValue::Array>(std::get<NodeValue::Object>(match.data).at("node_ids").data))
                ids.insert(std::get<std::uint64_t>(id.data));
            result.insert(ids);
        }
        return result;
    }
};
}

TEST_CASE("Alternation takes exactly one branch per walk", "[alternation][app]") {
    const Fixture fixture;
    const std::string query = "X->(A->B|C)->Y";
    REQUIRE(fixture.fromStart(query, 0) == NodeSets{{0, 1, 2, 3}});
    REQUIRE(fixture.fromStart(query, 10) == NodeSets{{10, 11, 12}});
    // Counter-examples: a partial branch (A without B) or a missing branch head.
    REQUIRE(fixture.fromStart(query, 20).empty());
    REQUIRE(fixture.fromStart(query, 30).empty());
    REQUIRE(fixture.fromStart(query, 50).empty());
    REQUIRE(fixture.search(query) == NodeSets{{0, 1, 2, 3}, {10, 11, 12}});
    // Reverse spelling of the same constraint.
    REQUIRE(fixture.search("Y<-(A|C)<-X") == NodeSets{{10, 11, 12}, {20, 21, 22}});
    REQUIRE(fixture.search("Y<-(A|C)<-X") == fixture.search("X->(A|C)->Y"));
}

TEST_CASE("An alternative may consume the last node of a walk", "[alternation][app]") {
    const Fixture fixture;
    // 91:C has no child: the walk reaches the group exit without a current node.
    REQUIRE(fixture.fromStart("X->(A|C)", 90) == NodeSets{{90, 91}});
    REQUIRE(fixture.fromStart("X->(A|C)+", 90) == NodeSets{{90, 91}});
    REQUIRE(fixture.fromStart("X->(A|C?)", 90) == NodeSets{{90}, {90, 91}});
    REQUIRE(fixture.search("X->(A->B|C)") == NodeSets{{0, 1, 2}, {10, 11}, {90, 91}});
    // Boundary right after the group: only the branch ending on a sink matches.
    REQUIRE(fixture.search("X->(A|C)->$") == NodeSets{{90, 91}});
    REQUIRE(fixture.search("X->(A->B|C)->Y->$") == NodeSets{{0, 1, 2, 3}, {10, 11, 12}});
}

TEST_CASE("Quantified alternation chooses again at every iteration", "[alternation][app]") {
    const Fixture fixture;
    auto result = PGInterpreterTG("X->(A|C)+->Y").interpret()->test(fixture.graph, {40}, fixture.predicates);
    REQUIRE(result->getMatches().size() == 1);
    REQUIRE(result->getMatches()[0]->getAll() == std::set<NodeId>{40, 41, 42, 43, 44});
    REQUIRE(result->getMatches()[0]->getWalks().at(0).quantifiers.at(0).count() == 3);
    REQUIRE(fixture.search("X->(A|C)+->Y") == NodeSets{{10, 11, 12}, {20, 21, 22}, {40, 41, 42, 43, 44}});
    // Optional group: the direct X->Y edge also matches; X->B->Y never does.
    REQUIRE(fixture.search("X->(A->B|C)?->Y") == NodeSets{{0, 1, 2, 3}, {10, 11, 12}, {50, 51}});
    // Nested alternation inside a branch.
    REQUIRE(fixture.search("X->((A|C)->B|C)->Y") == NodeSets{{0, 1, 2, 3}, {10, 11, 12}});
}

TEST_CASE("Alternation composes with anchors outside it, also from anchor roots", "[alternation][app]") {
    const Fixture fixture;
    // Rooted on X#: the forward part runs through the alternation.
    REQUIRE(fixture.search("X#->(A->B|C)->Y;X#->Z") == NodeSets{{60, 61, 62, 63, 64}});
    // Rooted on X#: the alternation precedes the root and is walked in reverse.
    // 80:B->81:X->82:Z is the counter-example.
    REQUIRE(fixture.search("(A|C)->X#;X#->Z") == NodeSets{{70, 71, 72}});
    REQUIRE(fixture.search("(A|B)->X#;X#->Z") == NodeSets{{80, 81, 82}});
}

TEST_CASE("Start planning reaches every alternative head", "[alternation][start]") {
    const Fixture fixture;
    std::vector<NodeId> nodes;
    for (const auto& [id, op] : fixture.nodes) nodes.push_back(id);
    const auto candidates = PGInterpreterTG("(A|C)->Y").interpret()->startCandidates(nodes, fixture.predicates);
    REQUIRE(candidates == std::vector<std::vector<std::optional<NodeId>>>{{1, 11, 21, 41, 42, 43, 61, 70, 91}});
    // A nullable alternative also lets the walk start on its successor.
    REQUIRE(PGInterpreterTG("(A|C?)->Y").interpret()->startCandidates(nodes, fixture.predicates) ==
            std::vector<std::vector<std::optional<NodeId>>>{{1, 3, 11, 12, 21, 22, 32, 41, 42, 43, 44, 51, 61, 63, 70, 91}});
    // Nothing mandatory at all: any node may start.
    REQUIRE(PGInterpreterTG("(A?|C?)").interpret()->startCandidates(nodes, fixture.predicates).at(0).size() == nodes.size());
}

TEST_CASE("Generation draws each alternative from the seed", "[alternation][generation]") {
    std::set<std::vector<std::string>> shapes;
    for (std::uint32_t seed = 0; seed < 40; ++seed) {
        INFO("seed " << seed);
        IRGenerationOptions options;
        options.seed = seed;
        const auto graph = PGInterpreterIR("X->(A->B|C)->Y", options).interpret();
        // Walk the generated chain from its single root.
        NodeNN::Ptr node;
        for (const auto& candidate : graph->getNodes()) if (candidate->getParents().empty()) node = candidate;
        std::vector<std::string> chain;
        for (; node; node = node->getChildren().empty() ? nullptr : *node->getChildren().begin())
            chain.push_back(node->getInfo<std::string>("Type"));
        REQUIRE((chain == std::vector<std::string>{"X", "A", "B", "Y"} || chain == std::vector<std::string>{"X", "C", "Y"}));
        shapes.insert(chain);
        // Same seed, same choice.
        REQUIRE(PGInterpreterIR("X->(A->B|C)->Y", options).interpret()->getNodes().size() == chain.size());
    }
    REQUIRE(shapes.size() == 2);
}
