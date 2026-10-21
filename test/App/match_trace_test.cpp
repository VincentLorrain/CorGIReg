#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"

using namespace CorGIReg;

namespace {
class TraceGraph final : public GraphView {
public:
    std::vector<NodeId> order;
    explicit TraceGraph(std::vector<NodeId> ids) : order(std::move(ids)) {}
    NodeIds parents(NodeId node) const override {
        auto i = find(node);
        return i == 0 ? NodeIds{} : NodeIds{order[i - 1]};
    }
    NodeIds children(NodeId node) const override {
        auto i = find(node);
        return i + 1 == order.size() ? NodeIds{} : NodeIds{order[i + 1]};
    }
    std::string nodeName(NodeId node) const override { return "source_" + std::to_string(node); }
private:
    std::size_t find(NodeId node) const {
        auto i = std::find(order.begin(), order.end(), node);
        if (i == order.end()) throw std::out_of_range("Unknown node");
        return i - order.begin();
    }
};
}

TEST_CASE("Match trace preserves query and source node execution order", "[match-trace][regression]") {
    auto graph = std::make_shared<TraceGraph>(std::vector<NodeId>{42, 0, 9});
    const std::string query = "TraceA+->TraceB";
    auto pattern = PGInterpreterTG(query).interpret();
    NodePredicates predicates{
        {"TraceA", [](NodeId id) { return id == 42 || id == 0; }},
        {"TraceB", [](NodeId id) { return id == 9; }}
    };
    auto result = pattern->test(graph, {42}, predicates);
    REQUIRE(result->getMatches().size() == 1);
    const auto& match = result->getMatches().front();
    REQUIRE(match->getQuery() == query);
    REQUIRE(match->getAll() == NodeIds{0, 9, 42});
    REQUIRE(match->getWalks().size() == 1);
    const auto& walk = match->getWalks().front();
    REQUIRE(walk.nodes.size() == 3);
    REQUIRE(walk.nodes[0].node == 42);
    REQUIRE(walk.nodes[1].node == 0);
    REQUIRE(walk.nodes[2].node == 9);
    REQUIRE(walk.nodes[0].predicate == "TraceA");
    REQUIRE(walk.nodes[2].predicate == "TraceB");
    REQUIRE(match->getNodeName(42) == "source_42");
    REQUIRE_FALSE(pattern->test(graph, {9}, predicates)->isMatch());
}
