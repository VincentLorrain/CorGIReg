#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "NodeTest/NodeDictionary.hpp"

#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;

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