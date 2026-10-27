#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <numeric>
#include "Utile/Utile.hpp"
#include <random>

using namespace CorGIReg;

namespace {
struct Fixture {
    std::string query;
    std::vector<std::string> types;
    std::vector<std::pair<NodeId, NodeId>> edges;
};

std::shared_ptr<ImportedGraph> makeGraph(const Fixture& fixture) {
    auto graph = std::make_shared<ImportedGraph>();
    for (std::size_t i = 0; i < fixture.types.size(); ++i)
        graph->addNode(i, fixture.types[i], "source_" + std::to_string(i));
    for (const auto& edge : fixture.edges) graph->addEdge(edge.first, edge.second);
    return graph;
}

NodePredicates predicatesFor(const std::shared_ptr<ImportedGraph>& graph) {
    NodePredicates predicates;
    for (const auto& entry : graph->getNodes()) {
        const auto type = entry.second.predicate;
        predicates.emplace(type, [graph, type](NodeId node) {
            return graph->getNodes().at(node).predicate == type;
        });
    }
    return predicates;
}

NodeIds allNodes(const ImportedGraph& graph) {
    NodeIds ids;
    for (const auto& entry : graph.getNodes()) ids.insert(entry.first);
    return ids;
}

// Small explicit graphs only: cover all starting tuples, including shared starts.
bool hasFullMatch(const std::shared_ptr<TGCompiled>& pattern,
                  const std::shared_ptr<ImportedGraph>& graph, NodePredicates predicates) {
    std::vector<std::optional<NodeId>> nodes;
    for (const auto& entry : graph->getNodes()) nodes.push_back(entry.first);
    if (nodes.empty()) nodes.push_back(std::nullopt);
    PermutationIterator<std::optional<NodeId>> starts(nodes, pattern->getNbStart());
    const auto expected = allNodes(*graph);
    while (starts.hasNext()) {
        const auto result = pattern->test(graph, starts.next(), predicates);
        for (const auto& match : result->getMatches())
            if (match->getAll() == expected) return true;
    }
    return false;
}

Fixture chain(std::string query, const std::vector<std::string>& types) {
    Fixture fixture{std::move(query), types, {}};
    for (std::size_t i = 1; i < types.size(); ++i) fixture.edges.emplace_back(i - 1, i);
    return fixture;
}
}

TEST_CASE("APP", "[app][regression]") {
    SECTION("Original pattern corpus with independently specified graphs") {
        // Keep every original corpus pattern, including both mixed-anchor cases.
        // Exact captures prevent a nullable '*' from passing on an empty submatch.
        const std::vector<Fixture> fixtures{
            chain("A->B", {"A", "B"}),
            chain("A<-B", {"B", "A"}),
            chain("A->B->C", {"A", "B", "C"}),
            chain("A->B->C->D", {"A", "B", "C", "D"}),
            {"A#->B;A#->C", {"A", "B", "C"}, {{0, 1}, {0, 2}}},
            {"$A$->B;$A$->C", {"A", "B", "C"}, {{0, 1}, {0, 2}}},
            {"A#->B+;A#->C*", {"A", "B", "B", "C", "C"}, {{0, 1}, {1, 2}, {0, 3}, {3, 4}}},
            {"(A#->B)*", {"A", "B", "B"}, {{0, 1}, {0, 2}}},
            {"(A#->B)+", {"A", "B", "B"}, {{0, 1}, {0, 2}}},
            {"(A#->B->C#)*", {"A", "B", "B", "C"}, {{0, 1}, {0, 2}, {1, 3}, {2, 3}}},
            {"(A#->B)+;A#->C", {"A", "B", "B", "C"}, {{0, 1}, {0, 2}, {0, 3}}},
            {"(A#->B#)+;A#->C->B#", {"A", "B", "C"}, {{0, 1}, {0, 2}, {2, 1}}},
            {"A#->B;A#->B", {"A", "B", "B"}, {{0, 1}, {0, 2}}}
        };
        for (const auto& fixture : fixtures) {
            INFO(fixture.query);
            const auto graph = makeGraph(fixture);
            const auto pattern = PGInterpreterTG(fixture.query).interpret();
            REQUIRE(hasFullMatch(pattern, graph, predicatesFor(graph)));
            REQUIRE_FALSE(hasFullMatch(pattern, graph, {}));
        }
    }

    SECTION("Empty graphs provide an absent start for nullable patterns") {
        auto empty = std::make_shared<ImportedGraph>();
        for (const std::string query : {"A*", "(A#->B)*", "(A#->B->C#)*", "A*;B?"}) {
            INFO(query);
            REQUIRE(hasFullMatch(PGInterpreterTG(query).interpret(), empty, {}));
        }
        REQUIRE_FALSE(hasFullMatch(PGInterpreterTG("A+").interpret(), empty, {}));
    }

    SECTION("Seeded bounded chains verify quantifier counts and negative cases") {
        constexpr unsigned seed = 20261022;
        INFO("graph seed = " << seed);
        std::mt19937 random(seed);
        for (int trial = 0; trial < 32; ++trial) {
            const auto a = random() % 4;
            const auto b = 1 + random() % 3;
            const auto c = random() % 2;
            std::vector<std::string> types(a, "AppA");
            types.insert(types.end(), b, "AppB");
            types.insert(types.end(), c, "AppC");
            const std::string query = trial % 2 == 0 ? "AppA*->AppB+->AppC?->$" : "$<-AppC?<-AppB+<-AppA*";
            INFO("trial = " << trial << ", query = " << query << ", counts = " << a << ',' << b << ',' << c);
            const auto graph = makeGraph(chain(query, types));
            auto predicates = predicatesFor(graph);
            const auto pattern = PGInterpreterTG(query).interpret();
            const auto result = pattern->test(graph, {0}, predicates);
            REQUIRE(result->getMatches().size() == 1);
            const auto& match = result->getMatches()[0];
            REQUIRE(match->getAll() == allNodes(*graph));
            REQUIRE(match->getQuery() == query);
            const auto& walk = match->getWalks()[0];
            REQUIRE(walk.quantifiers.size() == 3);
            REQUIRE(walk.quantifiers[0].count() == a);
            REQUIRE(walk.quantifiers[1].count() == b);
            REQUIRE(walk.quantifiers[2].count() == c);
            for (std::size_t i = 0; i < types.size(); ++i) {
                REQUIRE(walk.nodes[i].node == i);
                REQUIRE(walk.nodes[i].predicate == types[i]);
            }
            predicates["AppB"] = [](NodeId) { return false; };
            REQUIRE_FALSE(pattern->test(graph, {0}, predicates)->isMatch());
        }
    }

    SECTION("Seeded nested repetitions preserve each inner invocation") {
        constexpr unsigned seed = 5719;
        INFO("nested graph seed = " << seed);
        std::mt19937 random(seed);
        for (int trial = 0; trial < 16; ++trial) {
            const auto groups = 1 + random() % 3;
            std::vector<std::size_t> counts;
            std::vector<std::string> types;
            for (unsigned i = 0; i < groups; ++i) {
                const auto count = 1 + random() % 3;
                counts.push_back(count);
                types.insert(types.end(), count, "AppA"); types.push_back("AppB");
            }
            const auto fixture = chain("(AppA+->AppB)+->$", types);
            INFO("trial = " << trial << ", query = " << fixture.query);
            const auto graph = makeGraph(fixture);
            const auto result = PGInterpreterTG(fixture.query).interpret()->test(graph, {0}, predicatesFor(graph));
            REQUIRE(result->getMatches().size() == 1);
            const auto& match = result->getMatches()[0];
            REQUIRE(match->getAll() == allNodes(*graph));
            const auto& quantifiers = match->getWalks()[0].quantifiers;
            REQUIRE(quantifiers.size() == groups + 1);
            REQUIRE(quantifiers[0].count() == groups);
            for (std::size_t i = 0; i < counts.size(); ++i) {
                REQUIRE(quantifiers[i + 1].parent == 0);
                REQUIRE(quantifiers[i + 1].count() == counts[i]);
            }
        }
    }

    SECTION("A syntactically valid pattern need not admit a matching graph") {
        // An end assertion inside the mandatory group cannot be followed by B.
        // A random syntax generator is therefore not a positive-match oracle.
        const auto graph = makeGraph(chain("", {"AppA", "AppB"}));
        const auto impossible = PGInterpreterTG("(AppA->$)+->AppB").interpret();
        REQUIRE_FALSE(hasFullMatch(impossible, graph, predicatesFor(graph)));
        REQUIRE(hasFullMatch(PGInterpreterTG("AppA->AppB->$").interpret(), graph, predicatesFor(graph)));
    }
}
