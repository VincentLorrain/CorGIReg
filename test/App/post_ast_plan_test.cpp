#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"

using namespace CorGIReg;

TEST_CASE("Post AST proposal preserves public matches and quantifier traces", "[app][post-ast]") {
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(42, "A", "first");
    graph->addNode(0, "A", "second");
    graph->addNode(9, "B", "shared");
    graph->addNode(10, "C", "last");
    graph->addEdge(42, 0); graph->addEdge(0, 9); graph->addEdge(9, 10);
    NodePredicates predicates{
        {"A", [](NodeId id) { return id == 42 || id == 0; }},
        {"B", [](NodeId id) { return id == 9; }},
        {"C", [](NodeId id) { return id == 10; }}
    };
    PGInterpreterTG interpreter("A+->B#;B#->C");
    auto pattern = interpreter.interpret();
    // The current compiler does not promise lexical ordering of its start slots.
    const std::vector<std::vector<std::optional<NodeId>>> starts{{42, 9}, {9, 42}};
    bool matched = false, counted = false;
    for (const auto& assignment : starts) {
        auto matches = pattern->test(graph, assignment, predicates);
        matched |= matches->isMatch();
        for (const auto& match : matches->getMatches()) {
            REQUIRE(match->getAll() == NodeIds{0, 9, 10, 42});
            REQUIRE(match->getQuery() == "A+->B#;B#->C");
            for (const auto& walk : match->getWalks())
                for (const auto& q : walk.quantifiers) if (q.symbol == '+' && q.count() == 2) counted = true;
        }
    }
    REQUIRE(matched);
    REQUIRE(counted);
    predicates["C"] = [](NodeId) { return false; };
    for (const auto& assignment : starts)
        REQUIRE_FALSE(pattern->test(graph, assignment, predicates)->isMatch());
}
