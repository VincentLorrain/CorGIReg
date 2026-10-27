#include <catch2/catch_test_macros.hpp>
#include "GraphAccess/GraphView.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "NodeTest/NodeCondition.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"

using namespace CorGIReg;

TEST_CASE("NodeTest rules filter actual graph matches by arguments", "[app][rules]") {
    auto conv = std::make_shared<NodeNN>();
    auto end = std::make_shared<NodeNN>();
    conv->setInfo("kernel", std::vector<int>{3, 3});
    GraphNN source;
    source.addNode(conv);
    source.addNode(end);
    source.addEdge(conv, end);
    auto graph = std::make_shared<BasicGraphAdapter>(source);
    using V = NodeValue;
    NodeCondition condition("RuleConv3", V::Object{{"path", V::Array{"attributes", "kernel"}},
                                                   {"eq", V::Array{3, 3}}});
    NodePredicates predicates{{"RuleConv3", [condition, graph](NodeId id) { return condition.test(graph->node(id)); }},
                              {"RuleEnd", [](NodeId id) { return id == 1; }}};
    auto pattern = PGInterpreterTG("RuleConv3->RuleEnd").interpret();
    auto result = pattern->test(graph, {0}, predicates);
    REQUIRE(result->isMatch());
    REQUIRE(result->getMatches()[0]->getAll() == NodeIds{0, 1});
    conv->setInfo("kernel", std::vector<int>{1, 1});
    REQUIRE_FALSE(pattern->test(graph, {0}, predicates)->isMatch());
}

namespace {
// This graph owns only integer IDs and adjacency lists: no NodeNN instances.
class IntegerGraph final : public GraphView {
public:
    std::map<NodeId, NodeIds> incoming, outgoing;
    void add(NodeId id) { incoming[id]; outgoing[id]; }
    void edge(NodeId from, NodeId to) {
        add(from); add(to);
        outgoing[from].insert(to);
        incoming[to].insert(from);
    }
    NodeIds parents(NodeId id) const override { return incoming.at(id); }
    NodeIds children(NodeId id) const override { return outgoing.at(id); }
};
}

TEST_CASE("External graph returns original IDs without global predicates", "[graph-view]") {
    auto graph = std::make_shared<IntegerGraph>();
    graph->edge(0, 42);
    NodePredicates predicates{
        {"ExternalFirst", [](NodeId id) { return id == 0; }},
        {"ExternalLast", [](NodeId id) { return id == 42; }}
    };
    auto pattern = PGInterpreterTG("ExternalFirst->ExternalLast").interpret();
    auto result = pattern->test(graph, {0}, predicates);
    REQUIRE(result->isMatch());
    REQUIRE(result->getMatches().size() == 1);
    REQUIRE(result->getMatches()[0]->at("ExternalFirst") == NodeIds{0});
    REQUIRE(result->getMatches()[0]->at("ExternalLast") == NodeIds{42});
    REQUIRE(result->getMatches()[0]->getAll() == NodeIds{0, 42});
    REQUIRE(result->getGraph() == graph);
    REQUIRE_FALSE(pattern->test(graph, {42}, predicates)->isMatch());
    REQUIRE_FALSE(pattern->test(graph, {0}, {})->isMatch());
    REQUIRE(pattern->test(graph, {0}, predicates)->isMatch());
    REQUIRE_THROWS_AS(pattern->test(graph, {}, predicates), std::runtime_error);
    REQUIRE_THROWS_AS(pattern->test(nullptr, {0}, predicates), std::invalid_argument);
    REQUIRE_THROWS_AS(pattern->test(graph, {999}, predicates), std::out_of_range);

    auto reversed = PGInterpreterTG("ExternalLast<-ExternalFirst").interpret();
    REQUIRE(reversed->test(graph, {0}, predicates)->isMatch());
    REQUIRE_FALSE(reversed->test(graph, {42}, predicates)->isMatch());

    // Exercise actual parent traversal, not only the parser's '<-' reordering.
    auto backwards = std::make_shared<TransitionGraph>(
        TransitionTypes::UNIQUE, "ExternalLast", "backwards", 0);
    backwards->mergeOneStartOneValid(std::make_shared<TransitionGraph>(
        TransitionTypes::UNIQUE, "ExternalFirst", "backwards", 0));
    auto parentPattern = backwards->setTransitionChildToParent()->compile();
    auto parentResult = parentPattern->test(graph, {42}, predicates);
    REQUIRE(parentResult->isMatch());
    REQUIRE(parentResult->getMatches()[0]->getAll() == NodeIds{0, 42});
    REQUIRE_FALSE(parentPattern->test(graph, {0}, predicates)->isMatch());

    // Reuse the same compiled pattern on another graph, with its own predicates.
    auto second = std::make_shared<IntegerGraph>();
    second->edge(8, 9);
    NodePredicates secondPredicates{
        {"ExternalFirst", [](NodeId id) { return id == 8; }},
        {"ExternalLast", [](NodeId id) { return id == 9; }}
    };
    REQUIRE(pattern->test(second, {8}, secondPredicates)->isMatch());
    REQUIRE_FALSE(pattern->test(second, {8}, predicates)->isMatch());
}

TEST_CASE("External graph preserves common and strict branch semantics", "[graph-view]") {
    auto graph = std::make_shared<IntegerGraph>();
    graph->edge(0, 11);
    graph->edge(0, 22);
    graph->edge(7, 33);
    graph->edge(7, 44);
    NodePredicates predicates{
        {"ExternalAnchor", [](NodeId id) { return id == 0 || id == 7; }},
        {"ExternalLeft", [](NodeId id) { return id == 11 || id == 33; }},
        {"ExternalRight", [](NodeId id) { return id == 22 || id == 44; }}
    };
    for (const std::string suffix : {"#", "$"}) {
        INFO(suffix);
        const auto anchor = "ExternalAnchor" + suffix;
        auto pattern = PGInterpreterTG(anchor + "->ExternalLeft;" + anchor + "->ExternalRight").interpret();
        auto result = pattern->test(graph, {0, 0}, predicates);
        REQUIRE(result->isMatch());
        for (const auto& match : result->getMatches()) REQUIRE(match->getAll() == NodeIds{0, 11, 22});
        REQUIRE_FALSE(pattern->test(graph, {0, 7}, predicates)->isMatch());
        graph->edge(0, 99);
        REQUIRE(pattern->test(graph, {0, 0}, predicates)->isMatch() == (suffix == "#"));
        graph->outgoing[0].erase(99); graph->incoming[99].erase(0);
        REQUIRE(pattern->test(graph, {0, 0}, predicates)->isMatch());
    }
}

TEST_CASE("External graph handles anchors zero and terminal absence separately", "[graph-view]") {
    auto graph = std::make_shared<IntegerGraph>();
    graph->edge(0, 1);
    graph->edge(0, 2);
    NodePredicates predicates{
        {"ExternalRoot", [](NodeId id) { return id == 0; }},
        {"ExternalLeaf", [](NodeId id) { return id == 1 || id == 2; }}
    };
    auto repeated = PGInterpreterTG("($ExternalRoot$->ExternalLeaf)+").interpret();
    auto matches = repeated->test(graph, {0}, predicates);
    REQUIRE(matches->isMatch());
    for (const auto& match : matches->getMatches()) REQUIRE(match->getAll() == NodeIds{0, 1, 2});
    REQUIRE(PGInterpreterTG("ExternalLeaf->$").interpret()->test(graph, {1}, predicates)->isMatch());
    REQUIRE_FALSE(PGInterpreterTG("ExternalLeaf").interpret()->test(graph, {std::nullopt}, predicates)->isMatch());
    REQUIRE(PGInterpreterTG("ExternalLeaf?").interpret()->test(graph, {0}, predicates)->isMatch());
}

TEST_CASE("External graph lifetime follows returned matches", "[graph-view]") {
    auto graph = std::make_shared<IntegerGraph>();
    graph->add(0);
    std::weak_ptr<IntegerGraph> observer = graph;
    auto pattern = PGInterpreterTG("ExternalLifetime").interpret();
    auto result = pattern->test(graph, {0}, {{"ExternalLifetime", [](NodeId) { return true; }}});
    REQUIRE(result->isMatch());
    graph.reset();
    REQUIRE_FALSE(observer.expired());
    result.reset();
    REQUIRE(observer.expired());
}
