#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "NodeTest/NodeDictionary.hpp"

#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;

TEST_CASE("Common keys preserve the complete predicate name", "[regression]") {
    for (const std::string key : {"RegressionConv", "Regression_12"}) {
        NodeDictionary::registerFunction(key, [key](NodeNN::Ptr node) {
            return node->getType() == key;
        });
        auto matching = std::make_shared<NodeNN>();
        matching->setInfo("Type", key);
        auto different = std::make_shared<NodeNN>();
        different->setInfo("Type", std::string("Other"));

        for (const std::string suffix : {"#", "#2", "$", "$3"}) {
            INFO(key + suffix);
            auto compiled = PGInterpreterTG(key + suffix).interpret();
            REQUIRE(compiled->getTransitions().size() == 1);
            REQUIRE((*compiled->getTransitions().begin())->getTestKey() == key);
            REQUIRE(compiled->test({matching})->isMatch());
            REQUIRE_FALSE(compiled->test({different})->isMatch());
            REQUIRE(compiled->test({matching})->isMatch());
        }
    }
}

TEST_CASE("PGInterpreterTG") {

    SECTION("Parsing result") {

        
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return true; });
        NodeDictionary::registerFunction("B", [](NodeNN::Ptr node) { return true; });
        
        auto interpreter = PGInterpreterTG("A->B");
        auto transactionGraph = interpreter.interpret();

        REQUIRE(transactionGraph->getNbStart() == 1);
        REQUIRE(transactionGraph->getTransitions().size() == 2);
        for (auto transition : transactionGraph->getTransitions()) {
            REQUIRE(std::dynamic_pointer_cast<TransitionUnique>(transition) != nullptr);
        }


    }

}
