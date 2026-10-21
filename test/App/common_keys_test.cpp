#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeDictionary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"

using namespace CorGIReg;

TEST_CASE("Common keys share an anchor across branches", "[regression]") {
    NodeDictionary::registerFunction("RegressionAnchor", [](NodeNN::Ptr n) {
        return n->getType() == "RegressionAnchor";
    });
    NodeDictionary::registerFunction("RegressionLeaf", [](NodeNN::Ptr n) {
        return n->getType() == "RegressionLeaf";
    });
    auto anchor = std::make_shared<NodeNN>();
    anchor->setInfo("Type", std::string("RegressionAnchor"));
    auto otherAnchor = std::make_shared<NodeNN>();
    otherAnchor->setInfo("Type", std::string("RegressionAnchor"));
    GraphNN graph;
    for (auto parent : {anchor, otherAnchor}) {
        for (int i = 0; i < 2; ++i) {
            auto leaf = std::make_shared<NodeNN>();
            leaf->setInfo("Type", std::string("RegressionLeaf"));
            graph.addEdge(parent, leaf);
        }
    }
    for (const std::string suffix : {"#2", "$3"}) {
        auto branch = "RegressionAnchor" + suffix + "->RegressionLeaf";
        auto compiled = PGInterpreterTG(branch + ";" + branch).interpret();
        REQUIRE(compiled->getNbStart() == 2);
        REQUIRE(compiled->test({anchor, anchor})->isMatch());
        REQUIRE_FALSE(compiled->test({anchor, otherAnchor})->isMatch());

        auto unmatched = std::make_shared<NodeNN>();
        unmatched->setInfo("Type", std::string("Other"));
        graph.addEdge(anchor, unmatched);
        REQUIRE(compiled->test({anchor, anchor})->isMatch() == (suffix == "#2"));
        anchor->removeChild(unmatched);
        unmatched->removeParent(anchor);
    }
}

