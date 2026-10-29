#include "Cli/SearchExecutor.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace CorGIReg;
namespace {
void compareSearch(const std::shared_ptr<ImportedGraph>& graph, const std::string& query,
                   bool expectedMatch) {
    // Compile once: pointer-based start ordering can differ between compilations.
    auto pattern = PGInterpreterTG(query).interpret();
    NodePredicates predicates;
    std::vector<std::optional<NodeId>> candidates;
    for (const auto& entry : graph->getNodes()) {
        candidates.push_back(entry.first);
        const auto key = entry.second.predicate;
        predicates[key] = [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; };
    }
    if (candidates.empty()) candidates.push_back(std::nullopt);
    std::size_t count = 1;
    for (std::size_t i = 0; i < pattern->getNbStart(); ++i) count *= candidates.size();
    auto execute = [&](std::size_t jobs) {
        std::vector<std::shared_ptr<SolutionFromStartingPoint>> results(count);
        runOrderedSearch(count, jobs, [&](std::uint64_t index) {
            auto rest = index;
            std::vector<std::optional<NodeId>> starts;
            for (std::size_t i = 0; i < pattern->getNbStart(); ++i) {
                starts.push_back(candidates[rest % candidates.size()]);
                rest /= candidates.size();
            }
            results[index] = pattern->test(graph, starts, predicates);
            return std::string{};
        }, [](const auto&) {});
        return results;
    };
    const auto serial = execute(1);
    bool found = false;
    for (const auto& result : serial) found |= result->isMatch();
    REQUIRE(found == expectedMatch);
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        auto parallel = execute(4);
        for (std::size_t i = 0; i < count; ++i) {
            const auto& a = serial[i]->getMatches();
            const auto& b = parallel[i]->getMatches();
            REQUIRE(a.size() == b.size());
            for (std::size_t j = 0; j < a.size(); ++j) {
                REQUIRE(a[j]->getQuery() == b[j]->getQuery());
                REQUIRE(a[j]->getStartNode() == b[j]->getStartNode());
                REQUIRE(a[j]->getAll() == b[j]->getAll());
                REQUIRE(a[j]->getWalks().size() == b[j]->getWalks().size());
                for (auto id : a[j]->getAll()) REQUIRE(a[j]->getNodeName(id) == b[j]->getNodeName(id));
                for (std::size_t k = 0; k < a[j]->getWalks().size(); ++k) {
                    const auto& x = a[j]->getWalks()[k];
                    const auto& y = b[j]->getWalks()[k];
                    REQUIRE(x.origins == y.origins);
                    REQUIRE(x.nodes == y.nodes);
                    REQUIRE(x.quantifiers == y.quantifiers);
                }
            }
        }
    }
}
}

TEST_CASE("Parallel tuple search preserves nested ambiguous and reversed traces", "[parallel][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(42, "PA", "first");
    graph->addNode(0, "PA", "second");
    graph->addNode(9, "PA", "third");
    graph->addNode(71, "PB", "last");
    graph->addEdge(42, 0); graph->addEdge(0, 9); graph->addEdge(9, 71);
    for (const auto* query : {"PA+->PB", "(PA?)+->PB", "PA+->PA+->PB", "PB<-PA", "PA?",
                              "PA+->PB->$", "PA+->PB->Missing?->$", "$<-PB<-PA"}) {
        INFO(query);
        compareSearch(graph, query, true);
    }
    compareSearch(graph, "PA->Missing", false);
    compareSearch(graph, "PA+->$", false); // Every A suffix is followed by B.
}

TEST_CASE("Parallel tuple search isolates common and strict branch obligations", "[parallel][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    for (NodeId offset : {0, 100}) {
        graph->addNode(offset, "PA", "left");
        graph->addNode(offset + 1, "PB", "join");
        graph->addNode(offset + 2, "PC", "right");
        graph->addNode(offset + 3, "PD", "other");
        graph->addEdge(offset, offset + 1);
        graph->addEdge(offset + 1, offset + 2);
        graph->addEdge(offset + 1, offset + 3);
    }
    compareSearch(graph, "PA->PB#;PB#->PC", true);
    compareSearch(graph, "PA->$PB;PB#->PC", true);
    compareSearch(graph, "PA->PB$;PB#->PC", false);
    compareSearch(graph, "PA->$PB$;$PB$->PC", false);
    compareSearch(graph, "PA->$PB$;$PB$->PC;$PB$->PD", true);
    compareSearch(graph, "PA->PB#;PB#->Missing", false);
}

TEST_CASE("Parallel tuple search handles empty graphs and common cycles", "[parallel][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    compareSearch(graph, "PA*", true);
    compareSearch(graph, "PA+", false);
    // Nothing is guaranteed before these boundaries: refused patterns.
    REQUIRE_THROWS_AS(PGInterpreterTG("PA*->$").interpret(), std::invalid_argument);
    REQUIRE_THROWS_AS(PGInterpreterTG("($PA$)?->$").interpret(), std::invalid_argument);
    compareSearch(graph, "($PA$)+", false);
    graph->addNode(0, "PA", "cycle");
    graph->addNode(9, "PA", "other cycle");
    graph->addEdge(0, 0); graph->addEdge(9, 9);
    compareSearch(graph, "(PA#)+", true);
    compareSearch(graph, "($PA)+", true);
    compareSearch(graph, "(PA$)+", true);
    compareSearch(graph, "($PA$)+", true);
}
