#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
using namespace CorGIReg;

TEST_CASE("Numbered arrows retain their port in the lexer and AST", "[ports][parser]") {
    PartitionGraphLexer lexer("X->12Sub#");
    REQUIRE(lexer.getNextToken()->getLexeme() == "X");
    auto arrow = lexer.getNextToken();
    REQUIRE(arrow->getType() == tGTokensTypes::NEXT);
    REQUIRE(arrow->getLexeme() == "12");
    PartitionGraphParser parser("Sub#<-2X");
    REQUIRE(parser.parse()->getValue() == "2");
    REQUIRE(parser.parse()->getValue() == "2");
    for (const std::string query : {"X->0Y", "X->4294967296Y", "X->9999999999999999999999Y", "X->1"}) {
        INFO(query);
        REQUIRE_THROWS(PartitionGraphParser(query).parse());
    }
    REQUIRE(PartitionGraphParser("X-> 123").parse()->getChilds().at(1)->getValue() == "123");
}

TEST_CASE("Numbered arrows match actual operand ports in either textual direction", "[ports][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(0, "X", "x"); graph->addNode(9, "Sub", "subtract");
    graph->addEdge(0, 9, {1, 2}); graph->addEdge(0, 9, {2, 2});
    NodePredicates predicates{{"X", [](NodeId n) { return n == 0; }}, {"Sub", [](NodeId n) { return n == 9; }}};
    for (const std::string query : {"X->1Sub", "X->2Sub", "Sub<-1X", "X+->1Sub", "X*->1Sub", "X->1Sub?", "X->Sub"}) {
        INFO(query);
        auto result = PGInterpreterTG(query).interpret()->test(graph, {0}, predicates);
        REQUIRE_FALSE(result->getMatches().empty());
        for (const auto& match : result->getMatches()) REQUIRE(match->getAll() == NodeIds{0, 9});
    }
    for (const std::string query : {"X->3Sub", "Sub<-3X", "X?->1Sub"}) {
        INFO(query);
        REQUIRE(PGInterpreterTG(query).interpret()->test(graph, {9}, predicates)->getMatches().empty());
    }
    REQUIRE(PGInterpreterTG("X->3Sub").interpret()->test(graph, {0}, predicates)->getMatches().empty());
    auto shared = PGInterpreterTG("X#->1Sub#;X#->2Sub#").interpret();
    REQUIRE_FALSE(shared->test(graph, {0, 0}, predicates)->getMatches().empty());
    auto plain = std::make_shared<ImportedGraph>();
    plain->addNode(0, "X", "x"); plain->addNode(9, "Sub", "subtract"); plain->addEdge(0, 9);
    REQUIRE(PGInterpreterTG("X->1Sub").interpret()->test(plain, {0}, predicates)->getMatches().empty());
    REQUIRE_THROWS_AS(PGInterpreterTG("X->*Sub").interpret(), std::invalid_argument);
}

TEST_CASE("Generation assigns fixed and automatic input ports deterministically", "[ports][generation]") {
    IRGenerationOptions options;
    auto fixed = PGInterpreterIR("X#->1Sub#;X#->2Sub#", options).interpret();
    BasicGraphAdapter fixedView(*fixed);
    REQUIRE(fixedView.edgePorts(0, 1) == std::set<EdgePorts>{{1, std::nullopt}, {2, std::nullopt}});
    options.repetitions = {{1, {3}}};
    PGInterpreterIR generator("(A->*Sum#)+", options);
    auto generated = generator.interpret();
    auto again = generator.interpret();
    for (const auto& graph : {generated, again}) {
        REQUIRE(graph->getNodes().size() == 4);
        BasicGraphAdapter view(*graph);
        REQUIRE(view.edgePorts(0, 1) == std::set<EdgePorts>{{1, std::nullopt}});
        REQUIRE(view.edgePorts(2, 1) == std::set<EdgePorts>{{2, std::nullopt}});
        REQUIRE(view.edgePorts(3, 1) == std::set<EdgePorts>{{3, std::nullopt}});
    }
    REQUIRE_THROWS_AS(PGInterpreterIR("(A->1Sum#)+", options).interpret(), std::invalid_argument);
    options.repetitions.clear();
    REQUIRE_THROWS_AS(PGInterpreterIR("A->1Sum#;B->*Sum#", options).interpret(), std::invalid_argument);
    auto reverse = PGInterpreterIR("Sub<-2X", options).interpret();
    REQUIRE(BasicGraphAdapter(*reverse).edgePorts(1, 0) == std::set<EdgePorts>{{2, std::nullopt}});
    options.repetitions = {{1, {0}}};
    REQUIRE_THROWS_AS(PGInterpreterIR("A*->1B", options).interpret(), std::invalid_argument);
}

TEST_CASE("Seeded port fixtures distinguish correct swapped and absent operand numbers", "[ports][random]") {
    std::mt19937 random(20261026);
    for (unsigned sample = 0; sample < 100; ++sample) {
        PortNumber actual = 1 + random() % 16;
        PortNumber queried = 1 + random() % 16;
        INFO("seed=20261026 case=" << sample << " actual=" << actual << " queried=" << queried);
        auto graph = std::make_shared<ImportedGraph>();
        graph->addNode(0, "A", "a"); graph->addNode(1, "B", "b");
        graph->addEdge(0, 1, {actual, 1});
        NodePredicates predicates{{"A", [](NodeId id) { return id == 0; }}, {"B", [](NodeId id) { return id == 1; }}};
        auto query = "A->" + std::to_string(queried) + "B";
        REQUIRE(!PGInterpreterTG(query).interpret()->test(graph, {0}, predicates)->getMatches().empty() == (actual == queried));
    }
}
