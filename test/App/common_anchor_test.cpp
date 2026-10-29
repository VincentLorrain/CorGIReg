#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeDictionary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"

using namespace CorGIReg;

TEST_CASE("Repetition selects its own common anchor after other compilations", "[regression]") {
    for (const std::string anchorKey : {"OrderAnchorFirst", "OrderAnchorSecond"}) {
        const auto leafKey = anchorKey + "Leaf";
        REQUIRE(NodeDictionary::registerFunction(anchorKey, [anchorKey](NodeNN::Ptr node) {
            return node->getType() == anchorKey;
        }));
        REQUIRE(NodeDictionary::registerFunction(leafKey, [leafKey](NodeNN::Ptr node) {
            return node->getType() == leafKey;
        }));
        GraphNN graph;
        auto anchor = std::make_shared<NodeNN>();
        anchor->setInfo("Type", anchorKey);
        auto first = std::make_shared<NodeNN>();
        first->setInfo("Type", leafKey);
        auto second = std::make_shared<NodeNN>();
        second->setInfo("Type", leafKey);
        graph.addEdge(anchor, first);
        graph.addEdge(anchor, second);
        auto outgoing = PGInterpreterTG("($" + anchorKey + "$->" + leafKey + ")+").interpret();
        REQUIRE(outgoing->test({anchor})->isMatch());
        anchor->removeChild(first); first->removeParent(anchor);
        anchor->removeChild(second); second->removeParent(anchor);
        graph.addEdge(first, anchor);
        graph.addEdge(second, anchor);
        auto incoming = PGInterpreterTG("(" + leafKey + "->$" + anchorKey + "$)+").interpret();
        REQUIRE(incoming->test({first})->isMatch());
        REQUIRE_FALSE(incoming->test({anchor})->isMatch());
        first->removeChild(anchor); anchor->removeParent(first);
        second->removeChild(anchor); anchor->removeParent(second);
    }
}

TEST_CASE("Distinct numbered identities reject a node already bound to another one", "[regression][app]") {
    // D0 -> A1 -> C2 -> D0 and D0 -> A3. Walking back into D0 reaches A1 again as A#2.
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(0, "D", "d"); graph->addNode(1, "A", "a1"); graph->addNode(2, "C", "c"); graph->addNode(3, "A", "a3");
    graph->addEdge(0, 1); graph->addEdge(1, 2); graph->addEdge(2, 0); graph->addEdge(0, 3);
    auto predicatesOf = [](const std::shared_ptr<ImportedGraph>& source) {
        NodePredicates result;
        for (const std::string key : {"A", "C", "D"})
            result[key] = [source, key](NodeId id) { return source->getNodes().at(id).predicate == key; };
        return result;
    };
    const auto predicates = predicatesOf(graph);
    auto pattern = PGInterpreterTG("D#->A#1->C#->D#->A#2").interpret();
    std::set<NodeIds> found;
    for (NodeId start = 0; start < 4; ++start) {
        const auto result = pattern->test(graph, {start}, predicates);
        for (const auto& match : result->getMatches()) found.insert(match->getAll());
    }
    REQUIRE(found == std::set<NodeIds>{{0, 1, 2, 3}});
    // Without A3 the only remaining binding of A#2 is A1, which is rejected.
    auto cycle = std::make_shared<ImportedGraph>();
    cycle->addNode(0, "D", "d"); cycle->addNode(1, "A", "a1"); cycle->addNode(2, "C", "c");
    cycle->addEdge(0, 1); cycle->addEdge(1, 2); cycle->addEdge(2, 0);
    for (NodeId start = 0; start < 3; ++start)
        REQUIRE_FALSE(pattern->test(cycle, {start}, predicatesOf(cycle))->isMatch());
}

namespace {
std::shared_ptr<ImportedGraph> anchorGraph(const std::vector<std::string>& types,
                                           const std::vector<std::pair<NodeId, NodeId>>& edges) {
    auto graph = std::make_shared<ImportedGraph>();
    for (NodeId id = 0; id < types.size(); ++id) graph->addNode(id, types[id], types[id] + std::to_string(id));
    for (const auto& [from, to] : edges) graph->addEdge(from, to);
    return graph;
}
std::vector<std::shared_ptr<MatchSolution>> allMatches(const std::string& query, const std::shared_ptr<ImportedGraph>& graph) {
    NodePredicates predicates;
    for (const auto& entry : graph->getNodes()) {
        const auto key = entry.second.predicate;
        predicates[key] = [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; };
    }
    std::vector<std::shared_ptr<MatchSolution>> result;
    auto pattern = PGInterpreterTG(query).interpret();
    for (const auto& entry : graph->getNodes()) {
        const auto found = pattern->test(graph, {entry.first}, predicates);
        result.insert(result.end(), found->getMatches().begin(), found->getMatches().end());
    }
    return result;
}
}

TEST_CASE("Anchored repetitions restart at every distance of a variable prefix", "[regression][app]") {
    // Before, the next iteration restarted at one fixed distance from the anchor:
    // the skip path of A? (distance 0) or a single A for A+.
    const auto fork = anchorGraph({"A", "D", "A"}, {{0, 1}, {2, 1}});
    for (const std::string query : {"(A?->D#)+", "(A*->D#)+", "(A+->D#)+"}) {
        INFO(query);
        bool full = false;
        for (const auto& match : allMatches(query, fork)) full |= match->getAll() == NodeIds{0, 1, 2};
        REQUIRE(full);
    }
    // A0 -> A1 -> D2 <- A4 <- A3: two iterations of two A each.
    const auto chains = anchorGraph({"A", "A", "D", "A", "A"}, {{0, 1}, {1, 2}, {3, 4}, {4, 2}});
    bool counted = false;
    for (const auto& match : allMatches("(A+->D#)+", chains)) {
        if (match->getAll() != NodeIds{0, 1, 2, 3, 4}) continue;
        std::vector<std::pair<std::size_t, std::size_t>> counts;
        for (const auto& q : match->getWalks().at(0).quantifiers) counts.emplace_back(q.index, q.count());
        counted |= counts == std::vector<std::pair<std::size_t, std::size_t>>{{2, 2}, {1, 2}, {1, 2}};
    }
    REQUIRE(counted);
}

TEST_CASE("An anchor identity binds a single source node", "[regression][app]") {
    // Two separate A->D pairs are not one anchored repetition.
    const auto pairs = anchorGraph({"A", "D", "A", "D"}, {{0, 1}, {2, 3}});
    for (const std::string query : {"(A->D#)+", "(A*->D#)+", "(A?->D#)+"})
        for (const auto& match : allMatches(query, pairs)) {
            INFO(query);
            REQUIRE(match->getAll().size() <= 2);
        }
}
