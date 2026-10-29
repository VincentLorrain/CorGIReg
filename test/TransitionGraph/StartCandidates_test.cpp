#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <algorithm>

using namespace CorGIReg;

TEST_CASE("Start candidates keep only nodes a start can accept first", "[start][app]") {
    // 0:A 1:A 2:B 3:C
    auto graph = std::make_shared<ImportedGraph>();
    const std::vector<std::string> types{"A", "A", "B", "C"};
    NodePredicates predicates;
    for (NodeId id = 0; id < types.size(); ++id) {
        graph->addNode(id, types[id], "n");
        predicates[types[id]] = [graph, key = types[id]](NodeId node) { return graph->getNodes().at(node).predicate == key; };
    }
    const std::vector<NodeId> nodes{0, 1, 2, 3};
    using Candidates = std::vector<std::vector<std::optional<NodeId>>>;
    auto candidates = [&](const std::string& query) { return PGInterpreterTG(query).interpret()->startCandidates(nodes, predicates); };
    REQUIRE(candidates("A+->B") == Candidates{{0, 1}});
    REQUIRE(candidates("A?->B") == Candidates{{0, 1, 2}});              // B when A is skipped.
    REQUIRE(candidates("(A?)?") == Candidates{{0, 1, 2, 3}});           // Nullable: any node.
    REQUIRE(candidates("Missing") == Candidates{{}});                   // No predicate: nothing to try.
    auto paths = candidates("A#->B;C->A#");                              // Start order follows states.
    std::sort(paths.begin(), paths.end());
    REQUIRE(paths == Candidates{{0, 1}, {3}});
    auto pattern = PGInterpreterTG("A;B").interpret();
    REQUIRE(pattern->getStartGroups() == std::vector<std::size_t>{0, 1});
    REQUIRE(pattern->startCandidates({}, predicates) == Candidates{{std::nullopt}, {std::nullopt}});
}
