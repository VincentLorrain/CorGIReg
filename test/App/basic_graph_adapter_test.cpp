#include <catch2/catch_test_macros.hpp>
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include "NodeTest/NodeDictionary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"

using namespace CorGIReg;

TEST_CASE("Legacy and explicit graph views return the same source nodes", "[graph-view]") {
    GraphNN graph;
    auto first = std::make_shared<NodeNN>();
    auto last = std::make_shared<NodeNN>();
    first->setInfo("Type", std::string("LegacyFirst"));
    last->setInfo("Type", std::string("LegacyLast"));
    graph.addEdge(first, last);
    auto view = std::make_shared<BasicGraphAdapter>(graph);
    NodePredicates predicates;
    for (const std::string key : {"LegacyFirst", "LegacyLast"}) {
        REQUIRE(NodeDictionary::registerFunction(key, [key](NodeNN::Ptr node) {
            return node->getType() == key;
        }));
        predicates[key] = [view, key](NodeId id) { return view->node(id)->getType() == key; };
    }
    for (const std::string query : {"LegacyFirst->LegacyLast", "LegacyLast<-LegacyFirst",
                                   "LegacyFirst->LegacyLast->$", "LegacyFirst->LegacyLast?"}) {
        INFO(query);
        auto pattern = PGInterpreterTG(query).interpret();
        auto legacy = pattern->test({first});
        auto explicitView = pattern->test(view, {view->id(first)}, predicates);
        REQUIRE(legacy->isMatch());
        REQUIRE(explicitView->isMatch());
        auto source = std::dynamic_pointer_cast<const BasicGraphAdapter>(legacy->getGraph());
        REQUIRE(source);
        std::set<std::set<NodeNN::Ptr>> legacyNodes, explicitNodes;
        for (const auto& match : legacy->getMatches()) {
            std::set<NodeNN::Ptr> nodes;
            for (NodeId id : match->getAll()) nodes.insert(source->node(id));
            legacyNodes.insert(nodes);
        }
        for (const auto& match : explicitView->getMatches()) {
            std::set<NodeNN::Ptr> nodes;
            for (NodeId id : match->getAll()) nodes.insert(view->node(id));
            explicitNodes.insert(nodes);
        }
        REQUIRE(legacyNodes == explicitNodes);
        REQUIRE_FALSE(pattern->test({last})->isMatch());
        REQUIRE_FALSE(pattern->test(view, {view->id(last)}, predicates)->isMatch());
    }
    first->removeChild(last);
    last->removeParent(first);
}
