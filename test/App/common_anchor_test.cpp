#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeDictionary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"

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
        auto outgoing = PGInterpreterTG("(" + anchorKey + "$->" + leafKey + ")+").interpret();
        REQUIRE(outgoing->test({anchor})->isMatch());
        anchor->removeChild(first); first->removeParent(anchor);
        anchor->removeChild(second); second->removeParent(anchor);
        graph.addEdge(first, anchor);
        graph.addEdge(second, anchor);
        auto incoming = PGInterpreterTG("(" + leafKey + "->" + anchorKey + "$)+").interpret();
        REQUIRE(incoming->test({first})->isMatch());
        REQUIRE_FALSE(incoming->test({anchor})->isMatch());
        first->removeChild(anchor); anchor->removeParent(first);
        second->removeChild(anchor); anchor->removeParent(second);
    }
}
