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
