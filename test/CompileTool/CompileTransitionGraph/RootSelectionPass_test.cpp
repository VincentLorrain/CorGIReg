#include <catch2/catch_test_macros.hpp>
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"

using namespace CorGIReg;
namespace {
RootSelectionPlan plan(const std::string& query) {
    return RootSelectionPass{}.run(PartitionGraphParser(query).parse());
}
std::string rootName(const RootSelectionPlan& result, std::size_t component = 0) {
    return result.nodes.at(result.components.at(component).root.value()).occurrences.front()->getValue();
}
std::set<std::pair<std::string, std::string>> namedEdges(const RootSelectionPlan& result) {
    std::set<std::pair<std::string, std::string>> edges;
    for (const auto& edge : result.edges)
        edges.emplace(result.nodes[edge.source].occurrences.front()->getValue(),
                      result.nodes[edge.target].occurrences.front()->getValue());
    return edges;
}
std::string syntax(const std::shared_ptr<const PatternAst>& node) {
    std::string out = std::to_string(static_cast<int>(node->getType())) + ":" + node->getValue() + "(";
    for (const auto& child : node->getChilds()) out += syntax(child) + ",";
    return out + ")";
}
}

TEST_CASE("Post AST selection prefers anchors joining paths", "[post-ast]") {
    const auto result = plan("A+->B#;B#->C");
    REQUIRE(result.components.size() == 1);
    REQUIRE(rootName(result) == "B#");
    REQUIRE(result.nodes[*result.components[0].root].paths == std::set<std::size_t>{0, 1});
    REQUIRE(result.nodes[*result.components[0].root].occurrences.size() == 2);
    REQUIRE(result.components[0].status == RootSelectionStatus::Selected);
    const auto chain = plan("A->B#;B#->C#;C#->D");
    REQUIRE(chain.components.size() == 1);
    REQUIRE(rootName(chain) == "B#");
    REQUIRE(chain.nodes.size() == 4);
    REQUIRE(rootName(plan("A->B->C")) == "B");
}

TEST_CASE("Post AST selection preserves arrow constraints and cycles", "[post-ast]") {
    const auto forward = plan("A->B#;B#->C");
    const auto reverse = plan("B#<-A;C<-B#");
    REQUIRE(namedEdges(forward) == namedEdges(reverse));
    REQUIRE(rootName(reverse) == "B#");
    auto cycle = plan("A#->B#;B#->A#");
    REQUIRE(cycle.components.size() == 1);
    REQUIRE(cycle.edges.size() == 2);
    REQUIRE(rootName(cycle) == "A#");
    REQUIRE(namedEdges(cycle) == std::set<std::pair<std::string, std::string>>{{"A#", "B#"}, {"B#", "A#"}});
}

TEST_CASE("Post AST selection separates disconnected identities", "[post-ast]") {
    auto separate = plan("A->B;C->D");
    REQUIRE(separate.components.size() == 2);
    REQUIRE(rootName(separate, 0) == "A");
    REQUIRE(rootName(separate, 1) == "C");
    REQUIRE(plan("A;A").components.size() == 2);
    REQUIRE(plan("A#;A#0").components.size() == 2);
    auto aliases = plan("A#1;A#01;A$1");
    REQUIRE(aliases.components.size() == 1);
    REQUIRE(aliases.nodes.size() == 1);
    REQUIRE(aliases.nodes[0].occurrences.size() == 3);
    REQUIRE(aliases.nodes[0].occurrences.back()->getType() == tGTokensTypes::SCKEY);
}

TEST_CASE("Post AST selection keeps quantified syntax intact and defers unsafe roots", "[post-ast]") {
    auto tree = PartitionGraphParser("(A+->B)*->C->$").parse();
    auto before = syntax(tree);
    auto result = RootSelectionPass{}.run(tree);
    REQUIRE(syntax(tree) == before);
    REQUIRE(syntax(result.originalAst) == before);
    REQUIRE(rootName(result) == "C");
    REQUIRE(result.nodes.front().occurrences.front()->getType() == tGTokensTypes::QZM);
    for (const auto& query : {"A*", "A+", "A?", "(A->B)+"}) {
        auto deferred = plan(query);
        REQUIRE_FALSE(deferred.components[0].root.has_value());
        REQUIRE(deferred.components[0].status == RootSelectionStatus::NoEligibleRoot);
    }
    auto common = plan("(A#->B)+;A#->C");
    REQUIRE(common.hasQuantifiedCommon);
    for (const auto& component : common.components) {
        REQUIRE_FALSE(component.root.has_value());
        REQUIRE(component.status == RootSelectionStatus::QuantifiedCommonNeedsAnalysis);
    }
    REQUIRE_THROWS_AS(RootSelectionPass{}.run(nullptr), std::invalid_argument);
}

TEST_CASE("TG interpreter exposes a repeatable post AST plan before generation", "[post-ast]") {
    PGInterpreterTG interpreter("A+->B#;B#->C");
    REQUIRE(interpreter.getRootSelectionPlan().components.empty());
    auto first = interpreter.interpret();
    REQUIRE(rootName(interpreter.getRootSelectionPlan()) == "B#");
    const auto before = syntax(interpreter.getRootSelectionPlan().originalAst);
    auto second = interpreter.interpret();
    REQUIRE(rootName(interpreter.getRootSelectionPlan()) == "B#");
    REQUIRE(syntax(interpreter.getRootSelectionPlan().originalAst) == before);
    // Root selection is a proposal. The generation stage has not been rerooted yet.
    REQUIRE(first->getNbStart() == 2);
    REQUIRE(second->getNbStart() == 2);
    REQUIRE(first->getTransitions().size() == second->getTransitions().size());
}
