#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include <sstream>

using namespace CorGIReg;
namespace {
std::string topology(const std::shared_ptr<GraphNN>& graph) {
    BasicGraphAdapter adapter(*graph);
    std::ostringstream out;
    for (const auto& node : graph->getNodes()) {
        out << node->getType() << ':';
        for (auto child : adapter.children(adapter.id(node))) out << child << ',';
        out << ';';
    }
    return out.str();
}
}

TEST_CASE("Deterministic IR expands lexical quantifiers per invocation", "[generation]") {
    IRGenerationOptions options;
    options.repetitions = {{1, {3, 1}}, {2, {2}}, {3, {0}}, {4, {1}}};
    PGInterpreterIR generator("(A+->B)+->C*->D?", options);
    auto graph = generator.interpret();
    REQUIRE(topology(graph) == "A:1,;A:2,;A:3,;B:4,;A:5,;B:6,;D:;");
    REQUIRE(topology(generator.interpret()) == topology(graph));
    for (const auto& node : graph->getNodes())
        for (const auto& child : node->getChildren()) REQUIRE(child->getParents().count(node) == 1);

    auto adapter = std::make_shared<BasicGraphAdapter>(*graph);
    NodePredicates predicates;
    for (const std::string key : {"A", "B", "C", "D"})
        predicates[key] = [adapter, key](NodeId id) { return adapter->node(id)->getType() == key; };
    auto machine = PGInterpreterTG("(A+->B)+->C*->D?").interpret();
    REQUIRE_FALSE(machine->test(adapter, {0}, predicates)->getMatches().empty());
    REQUIRE(PGInterpreterTG("A->D").interpret()->test(adapter, {0}, predicates)->getMatches().empty());
}

TEST_CASE("Deterministic IR preserves reversed links shared anchors and cycles", "[generation]") {
    IRGenerationOptions options;
    REQUIRE(topology(PGInterpreterIR("A<-B<-C", options).interpret()) == "A:;B:0,;C:1,;");
    REQUIRE(topology(PGInterpreterIR("A#->B;A#->C", options).interpret()) == "A:1,2,;B:;C:;");
    REQUIRE(topology(PGInterpreterIR("$A$->A#", options).interpret()) == "A:0,;");
    options.repetitions = {{1, {2}}};
    REQUIRE(topology(PGInterpreterIR("(A#->B)+", options).interpret()) == "A:1,2,;B:;B:;");
    REQUIRE(topology(PGInterpreterIR("(A<-B)+", options).interpret()) == "A:3,;B:0,;A:;B:2,;");
}

TEST_CASE("Deterministic IR validates values and bounds expansion", "[generation]") {
    IRGenerationOptions options;
    REQUIRE_THROWS_AS(PGInterpreterIR("A+", options).interpret(), std::invalid_argument);
    for (const auto& values : std::vector<std::map<std::size_t, std::vector<std::size_t>>>{
             {{0, {1}}}, {{2, {1}}}, {{1, {}}}, {{1, {0}}}, {{1, {1, 2}}}, {{1, {1000001}}}}) {
        options.repetitions = values;
        REQUIRE_THROWS_AS(PGInterpreterIR("A+", options).interpret(), std::invalid_argument);
    }
    options.repetitions = {{1, {2}}};
    REQUIRE_THROWS_AS(PGInterpreterIR("A?", options).interpret(), std::invalid_argument);
    options.repetitions = {{1, {1, 2}}, {2, {3}}};
    REQUIRE_THROWS_AS(PGInterpreterIR("(A+)+", options).interpret(), std::invalid_argument);
    options.repetitions = {{1, {3}}}; options.maxNodes = 2;
    REQUIRE_THROWS_AS(PGInterpreterIR("A+", options).interpret(), std::length_error);
}

TEST_CASE("Configured random IR is reproducible and explicit values take precedence", "[generation]") {
    IRGenerationOptions options;
    options.randomMissing = true; options.seed = 20261026; options.maxLoops = 4;
    options.repetitions = {{1, {3}}};
    PGInterpreterIR generator("A+->B*->C?", options);
    auto first = generator.interpret();
    REQUIRE(topology(first) == topology(generator.interpret()));
    REQUIRE(topology(first) == topology(PGInterpreterIR("A+->B*->C?", options).interpret()));
    std::size_t count = 0;
    for (const auto& node : first->getNodes()) if (node->getType() == "A") ++count;
    REQUIRE(count == 3);
    options.repetitions.clear(); options.maxLoops = 0;
    REQUIRE(topology(PGInterpreterIR("A+->B*", options).interpret()) == "A:;");
}


