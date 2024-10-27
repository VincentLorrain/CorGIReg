#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "NodeTest/NodeDictionary.hpp"


#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;

TEST_CASE("PGInterpreterIR") {

    SECTION("Parsing sequence") {

 
        auto interpreter = PGInterpreterIR("A->B");
        auto graph = interpreter.interpret();

        for (auto node : graph->getNodes()) {
            if (node->getInfo<std::string>("Type") == "A") {
                REQUIRE(node->getChildren().size() == 1);
                REQUIRE(node->getParents().size() == 0);
            } else if (node->getInfo<std::string>("Type") == "B") {
                REQUIRE(node->getChildren().size() == 0);
                REQUIRE(node->getParents().size() == 1);
            } else {
                REQUIRE(false);
            }
        }

    }


    SECTION("Parsing Common") {

 
        auto interpreter = PGInterpreterIR("A#->B;A#->C");
        auto graph = interpreter.interpret();

        for (auto node : graph->getNodes()) {
            if (node->getInfo<std::string>("Type") == "A" && node->getInfo<bool>("isCommon")) {
                REQUIRE(node->getChildren().size() == 2);
                REQUIRE(node->getParents().size() == 0);
            } else if (node->getInfo<std::string>("Type") == "B") {
                REQUIRE(node->getChildren().size() == 0);
                REQUIRE(node->getParents().size() == 1);
            } else if (node->getInfo<std::string>("Type") == "C") {
                REQUIRE(node->getChildren().size() == 0);
                REQUIRE(node->getParents().size() == 1);
            } else {
                REQUIRE(false);
            }
        }

    }


}