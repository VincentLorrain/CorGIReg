#include <catch2/catch_test_macros.hpp>
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
using namespace CorGIReg;

TEST_CASE("Numbered connections preserve repeated operands through copies and merges", "[ports]") {
    GraphNN graph;
    auto x = std::make_shared<NodeNN>(), sub = std::make_shared<NodeNN>();
    graph.addEdge(x, sub, {1, 1}); graph.addEdge(x, sub, {2, 1});
    graph.addEdge(sub, sub, {3, 1});
    BasicGraphAdapter view(graph);
    REQUIRE(view.children(0) == NodeIds{1});
    REQUIRE(view.edgePorts(0, 1) == std::set<EdgePorts>{{1, 1}, {2, 1}});
    REQUIRE(view.edgePorts(1, 0).empty());
    auto copy = graph.deepCopy({});
    BasicGraphAdapter copied(*copy);
    REQUIRE(copied.edgePorts(0, 1) == view.edgePorts(0, 1));
    REQUIRE(copied.edgePorts(1, 1) == std::set<EdgePorts>{{3, 1}});
    graph.mergeNodes({sub});
    BasicGraphAdapter merged(graph);
    REQUIRE(merged.edgePorts(0, 1) == std::set<EdgePorts>{{1, 1}, {2, 1}});
    REQUIRE(merged.edgePorts(1, 1) == std::set<EdgePorts>{{3, 1}});
    REQUIRE_THROWS_AS(graph.addEdge(x, sub, {0, 1}), std::invalid_argument);
}

TEST_CASE("Imported connections keep optional ports without changing adjacency", "[ports]") {
    ImportedGraph graph;
    graph.addNode(0, "X", "x"); graph.addNode(9, "Sub", "s");
    graph.addEdge(0, 9); graph.addEdge(0, 9, {1, 2}); graph.addEdge(0, 9, {2, 2});
    REQUIRE(graph.parents(9) == NodeIds{0});
    REQUIRE(graph.edgePorts(0, 9).size() == 3);
    REQUIRE_THROWS_AS(graph.addEdge(0, 9, {1, 0}), std::invalid_argument);
}
