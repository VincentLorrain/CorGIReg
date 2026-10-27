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

class BranchTraceGraph final : public GraphView {
public:
    std::map<NodeId, NodeIds> incoming, outgoing;
    void edge(NodeId from, NodeId to) {
        incoming[from]; outgoing[to];
        incoming[to].insert(from); outgoing[from].insert(to);
    }
    NodeIds parents(NodeId node) const override { return incoming.at(node); }
    NodeIds children(NodeId node) const override { return outgoing.at(node); }
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
    REQUIRE(walk.quantifiers.size() == 1);
    REQUIRE(walk.quantifiers[0].index == 1);
    REQUIRE(walk.quantifiers[0].symbol == '+');
    REQUIRE(walk.quantifiers[0].count() == 2);
    REQUIRE(walk.quantifiers[0].begin == 0);
    REQUIRE(walk.quantifiers[0].end == 2);
    REQUIRE(walk.quantifiers[0].iterationEnds == std::vector<std::size_t>{1, 2});
    REQUIRE_FALSE(pattern->test(graph, {9}, predicates)->isMatch());
}

TEST_CASE("Quantifier traces distinguish zero optional and nested invocations", "[match-trace][regression]") {
    auto graph = std::make_shared<TraceGraph>(std::vector<NodeId>{42, 0, 9, 8, 7});
    NodePredicates predicates{
        {"TraceA", [](NodeId id) { return id == 42 || id == 0 || id == 8; }},
        {"TraceB", [](NodeId id) { return id == 9 || id == 7; }}
    };
    SECTION("Nested counts retain the parent invocation and iteration boundaries") {
        auto interpreter = PGInterpreterTG("(TraceA+->TraceB)+->$");
        for (int run = 0; run < 2; ++run) {
            auto result = interpreter.interpret()->test(graph, {42}, predicates);
            REQUIRE(result->getMatches().size() == 1);
            const auto& walk = result->getMatches()[0]->getWalks()[0];
            REQUIRE(walk.nodes.size() == 5);
            const auto& q = walk.quantifiers;
            REQUIRE(q.size() == 3);
            REQUIRE(q[0].index == 2);
            REQUIRE(q[0].count() == 2);
            REQUIRE_FALSE(q[0].parent.has_value());
            REQUIRE(q[0].iterationEnds == std::vector<std::size_t>{3, 5});
            REQUIRE(q[1].index == 1);
            REQUIRE(q[1].count() == 2);
            REQUIRE(q[1].parent == 0);
            REQUIRE(q[2].index == 1);
            REQUIRE(q[2].count() == 1);
            REQUIRE(q[2].parent == 0);
            REQUIRE(q[2].begin == 3);
            REQUIRE(q[2].end == 4);
        }
    }
    SECTION("Zero and optional are explicit even at graph end") {
        auto terminal = std::make_shared<TraceGraph>(std::vector<NodeId>{7});
        for (const std::string symbol : {"*", "?"}) {
            auto pattern = PGInterpreterTG("TraceA" + symbol + "->TraceB->TraceA?").interpret();
            auto result = pattern->test(terminal, {7}, predicates);
            REQUIRE(result->getMatches().size() == 1);
            const auto& q = result->getMatches()[0]->getWalks()[0].quantifiers;
            REQUIRE(q.size() == 2);
            REQUIRE(q[0].index == 1);
            REQUIRE(q[0].count() == 0);
            REQUIRE(q[0].begin == q[0].end);
            REQUIRE(q[1].index == 2);
            REQUIRE(q[1].count() == 0);
        }
        auto absent = PGInterpreterTG("TraceA*").interpret()->test(terminal, {std::nullopt}, predicates);
        REQUIRE(absent->isMatch());
        REQUIRE(absent->getMatches()[0]->getWalks()[0].quantifiers[0].count() == 0);
        REQUIRE_FALSE(PGInterpreterTG("TraceA+").interpret()->test(terminal, {std::nullopt}, predicates)->isMatch());
        auto present = PGInterpreterTG("TraceB?").interpret()->test(terminal, {7}, predicates);
        std::set<std::size_t> counts;
        for (const auto& match : present->getMatches()) counts.insert(match->getWalks()[0].quantifiers[0].count());
        REQUIRE(counts == std::set<std::size_t>{0, 1});
    }
    SECTION("Lexical indices do not change when graph traversal is reversed") {
        auto result = PGInterpreterTG("TraceB?<-TraceA+").interpret()->test(graph, {42}, predicates);
        REQUIRE(result->isMatch());
        bool full = false;
        for (const auto& match : result->getMatches()) {
            const auto& walk = match->getWalks()[0];
            if (walk.nodes.size() != 3) continue;
            REQUIRE(walk.quantifiers.size() == 2);
            REQUIRE(walk.quantifiers[0].index == 2);
            REQUIRE(walk.quantifiers[0].count() == 2);
            REQUIRE(walk.quantifiers[1].index == 1);
            REQUIRE(walk.quantifiers[1].count() == 1);
            full = true;
        }
        REQUIRE(full);
    }
}

TEST_CASE("Quantifier ambiguity retains different allocations of the same nodes", "[match-trace][regression]") {
    auto graph = std::make_shared<TraceGraph>(std::vector<NodeId>{42, 0, 9});
    NodePredicates predicates{{"TraceA", [](NodeId) { return true; }}};
    auto result = PGInterpreterTG("TraceA+->TraceA+->$").interpret()->test(graph, {42}, predicates);
    REQUIRE(result->getMatches().size() == 2);
    std::set<std::pair<std::size_t, std::size_t>> allocations;
    for (const auto& match : result->getMatches()) {
        const auto& walk = match->getWalks()[0];
        REQUIRE(walk.nodes.size() == 3);
        REQUIRE(walk.quantifiers.size() == 2);
        allocations.emplace(walk.quantifiers[0].count(), walk.quantifiers[1].count());
    }
    REQUIRE(allocations == std::set<std::pair<std::size_t, std::size_t>>{{1, 2}, {2, 1}});
}

TEST_CASE("Nullable repetition produces finite canonical traces", "[match-trace][regression]") {
    auto graph = std::make_shared<TraceGraph>(std::vector<NodeId>{42});
    NodePredicates predicates{{"TraceA", [](NodeId) { return false; }}};
    auto result = PGInterpreterTG("(TraceA?)*").interpret()->test(graph, {42}, predicates);
    REQUIRE(result->getMatches().size() == 2);
    for (const auto& match : result->getMatches()) {
        const auto& walk = match->getWalks()[0];
        REQUIRE(walk.nodes.empty());
        REQUIRE(walk.quantifiers[0].count() <= 1);
    }
}

TEST_CASE("Common node iterations count bodies rather than distinct source nodes", "[match-trace][regression]") {
    auto graph = std::make_shared<BranchTraceGraph>();
    graph->edge(0, 11); graph->edge(0, 22);
    NodePredicates predicates{
        {"TraceAnchor", [](NodeId id) { return id == 0; }},
        {"TraceLeaf", [](NodeId id) { return id == 11 || id == 22; }}
    };
    auto result = PGInterpreterTG("($TraceAnchor$->TraceLeaf)+").interpret()->test(graph, {0}, predicates);
    REQUIRE(result->isMatch());
    for (const auto& match : result->getMatches()) {
        REQUIRE(match->getAll() == NodeIds{0, 11, 22});
        REQUIRE(match->getNodeName(0) == "0");
        const auto& walk = match->getWalks()[0];
        REQUIRE(walk.nodes.size() == 4);
        REQUIRE(walk.nodes[0].node == 0);
        REQUIRE(walk.nodes[2].node == 0);
        REQUIRE(walk.quantifiers.size() == 1);
        REQUIRE(walk.quantifiers[0].count() == 2);
        REQUIRE(walk.quantifiers[0].iterationEnds == std::vector<std::size_t>{2, 4});
    }
    auto separated = PGInterpreterTG("TraceAnchor#->TraceLeaf+;TraceAnchor#->TraceLeaf+")
        .interpret()->test(graph, {0, 0}, predicates);
    REQUIRE(separated->isMatch());
    for (const auto& match : separated->getMatches()) {
        REQUIRE(match->getWalks().size() == 2);
        std::set<std::size_t> indices;
        for (const auto& walk : match->getWalks()) {
            REQUIRE(walk.quantifiers.size() == 1);
            REQUIRE(walk.quantifiers[0].count() == 1);
            indices.insert(walk.quantifiers[0].index);
        }
        REQUIRE(indices == std::set<std::size_t>{1, 2});
    }
}

TEST_CASE("Programmatic common cycles still terminate without quantifier metadata", "[match-trace][regression]") {
    auto graph = std::make_shared<BranchTraceGraph>();
    graph->edge(0, 0);
    auto program = std::make_shared<TransitionGraph>(TransitionTypes::COMMON, "TraceAnchor#", "cycle", 0);
    auto pattern = program->addTransitionValidToStart(TransitionTypes::EMPTY, "")->compile();
    auto result = pattern->test(graph, {0}, {{"TraceAnchor", [](NodeId) { return true; }}});
    REQUIRE(result->getMatches().size() == 1);
    REQUIRE(result->getMatches()[0]->getAll() == NodeIds{0});
    REQUIRE(result->getMatches()[0]->getWalks()[0].quantifiers.empty());
}
