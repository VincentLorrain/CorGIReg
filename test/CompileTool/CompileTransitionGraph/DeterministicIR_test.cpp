#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include <sstream>
#include <filesystem>

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

TEST_CASE("Repeated anchored groups connect outer neighbors to one iteration", "[generation][regression]") {
    // Wiring X to every iteration gave the plain X two children or parents,
    // so the generated graph could not match its own pattern.
    IRGenerationOptions options;
    options.repetitions = {{1, {2}}};
    REQUIRE(topology(PGInterpreterIR("X->(A->D#)+", options).interpret()) == "X:1,;A:2,;D:;A:2,;");
    REQUIRE(topology(PGInterpreterIR("(A#->B)+->X", options).interpret()) == "A:1,2,;B:;B:3,;X:;");
    REQUIRE(topology(PGInterpreterIR("(A->D#)+<-X", options).interpret()) == "A:1,;D:;A:1,;X:0,;");
    REQUIRE(topology(PGInterpreterIR("X#->(A->D#)+", options).interpret()) == "X:1,;A:2,;D:;A:2,;");
    for (const std::string query : {"X->(A->D#)+", "(A#->B)+->X", "(A->D#)+<-X", "Y->(B->A#->C)+->Z"}) {
        INFO(query);
        auto graph = PGInterpreterIR(query, options).interpret();
        auto view = std::make_shared<BasicGraphAdapter>(*graph);
        NodePredicates predicates;
        for (const auto& node : graph->getNodes()) {
            const auto type = node->getType();
            predicates[type] = [view, type](NodeId id) { return view->node(id)->getType() == type; };
        }
        NodeIds all;
        for (NodeId id = 0; id < graph->getNodes().size(); ++id) all.insert(id);
        auto pattern = PGInterpreterTG(query).interpret();
        bool full = false;
        for (NodeId start = 0; start < graph->getNodes().size(); ++start) {
            const auto result = pattern->test(view, {start}, predicates);
            for (const auto& match : result->getMatches()) full |= match->getAll() == all;
        }
        REQUIRE(full);
    }
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

TEST_CASE("IR value files load counts modes and seeds with useful errors", "[generation]") {
    const auto example = std::filesystem::path(__FILE__).parent_path() /
        "../../../examples/generation/nested.values";
    REQUIRE(topology(PGInterpreterIR("(A+->B)+->C*->D?", example.string()).interpret()) ==
        "A:1,;A:2,;A:3,;B:4,;A:5,;B:6,;D:;");
    std::istringstream random("mode = random\nseed = 42\nmax_loops = 0\nmax_nodes = 4\n1 = 3 # fixed\n");
    auto options = IRGenerationOptions::read(random);
    REQUIRE(options.randomMissing);
    REQUIRE(options.seed == 42);
    REQUIRE(options.maxNodes == 4);
    REQUIRE(topology(PGInterpreterIR("A+->B*", options).interpret()) == "A:1,;A:2,;A:;");
    REQUIRE_THROWS_AS(IRGenerationOptions::fromFile(example.string() + ".missing"), std::runtime_error);
    for (const std::string text : {"1=-1", "1=1,", "0=1", "1=", "1=2\n01=3", "seed=1\nseed=2",
                                  "mode=other", "typo=2", "1 2", "1=9999999999999999999999999", "seed=4294967296"}) {
        INFO(text);
        std::istringstream input(text);
        REQUIRE_THROWS_AS(IRGenerationOptions::read(input), std::invalid_argument);
    }
}

TEST_CASE("IR parameter series has independent node and edge expectations", "[generation][series]") {
    for (std::size_t length : {1u, 2u, 8u, 32u}) {
        for (std::size_t optional : {0u, 1u}) {
            INFO("length=" << length << " optional=" << optional);
            IRGenerationOptions options;
            options.repetitions = {{1, {length}}, {2, {optional}}, {3, {2}}};
            auto graph = PGInterpreterIR("A+->B?->C*->$", options).interpret();
            REQUIRE(graph->getNodes().size() == length + optional + 2);
            BasicGraphAdapter adapter(*graph);
            for (std::size_t id = 0; id < graph->getNodes().size(); ++id) {
                REQUIRE(adapter.node(id)->getType() == (id < length ? "A" : (id < length + optional ? "B" : "C")));
                REQUIRE(adapter.parents(id) == (id ? NodeIds{id - 1} : NodeIds{}));
                REQUIRE(adapter.children(id) == (id + 1 < graph->getNodes().size() ? NodeIds{id + 1} : NodeIds{}));
            }
        }
    }
}


