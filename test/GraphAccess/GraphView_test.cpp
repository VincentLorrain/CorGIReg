#include <catch2/catch_test_macros.hpp>
#include "GraphAccess/GraphTraversal.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"

using namespace CorGIReg;

TEST_CASE("Graph view preserves source identities and signed traversal", "[graph-view]") {
    GraphNN graph;
    std::vector<NodeNN::Ptr> nodes;
    for (int i = 0; i < 4; ++i) {
        nodes.push_back(std::make_shared<NodeNN>());
        graph.addNode(nodes.back());
    }
    graph.addEdge(nodes[0], nodes[1]);
    graph.addEdge(nodes[0], nodes[2]);
    graph.addEdge(nodes[1], nodes[2]);
    graph.addEdge(nodes[2], nodes[3]);
    graph.addEdge(nodes[3], nodes[0]);
    BasicGraphAdapter view(graph);
    REQUIRE(view.node(0) == nodes[0]);
    REQUIRE(view.id(nodes[3]) == 3);
    REQUIRE(view.children(0) == NodeIds{1, 2});
    REQUIRE(view.parents(2) == NodeIds{0, 1});
    REQUIRE(nodesAtDelta(view, 0, 0) == NodeIds{0});
    REQUIRE(nodesAtDelta(view, 0, 2) == NodeIds{2, 3});
    REQUIRE(nodesAtDelta(view, 3, -2) == NodeIds{0, 1});
    REQUIRE(nodesAtDelta(view, 0, 4).empty());
    REQUIRE_THROWS_AS(view.children(99), std::out_of_range);
    for (int delta = -4; delta <= 4; ++delta) {
        NodeIds expected;
        for (const auto& node : nodes[0]->getNodeDelta(delta, {})) expected.insert(view.id(node));
        REQUIRE(nodesAtDelta(view, 0, delta) == expected);
    }
    // Break the explicitly created ownership cycle after checking the view.
    for (const auto& node : nodes) {
        auto children = node->getChildren();
        for (const auto& child : children) {
            node->removeChild(child);
            child->removeParent(node);
        }
    }
}
