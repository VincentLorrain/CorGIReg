#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "NodeTest/NodeDictionary.hpp"


#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;

TEST_CASE("IR repetition accepts a zero upper bound", "[regression]") {
    for (const std::string query : {"A*", "(A->B)*", "(Anchor#->B)*"}) {
        INFO(query);
        REQUIRE(PGInterpreterIR(query, 0).interpret()->getNodes().empty());
    }
    auto sequence = PGInterpreterIR("A->B*->C", 0).interpret();
    REQUIRE(sequence->getNodes().size() == 2);
    for (const auto& node : sequence->getNodes()) {
        if (node->getType() == "A") {
            REQUIRE(node->getChildren().size() == 1);
            REQUIRE((*node->getChildren().begin())->getType() == "C");
        } else {
            REQUIRE(node->getType() == "C");
            REQUIRE(node->getParents().size() == 1);
        }
    }
    auto mandatory = PGInterpreterIR("(A->B)+", 0).interpret();
    REQUIRE(mandatory->getNodes().size() == 2);
    for (const auto& node : mandatory->getNodes()) {
        if (node->getType() == "A") {
            REQUIRE(node->getChildren().size() == 1);
            REQUIRE((*node->getChildren().begin())->getType() == "B");
        }
    }
}

TEST_CASE("IR mandatory repetition retains common anchors", "[regression]") {
    PGInterpreterIR interpreter("(Anchor#->B)+;Anchor#->C", 0);
    auto first = interpreter.interpret();
    auto second = interpreter.interpret();
    for (const auto& graph : {first, second}) {
        REQUIRE(graph->getNodes().size() == 3);
        for (const auto& node : graph->getNodes()) {
            if (node->getType() == "Anchor") {
                REQUIRE(node->getChildren().size() == 2);
                REQUIRE(node->getParents().empty());
            } else {
                REQUIRE(node->getParents().size() == 1);
                REQUIRE((*node->getParents().begin())->getType() == "Anchor");
            }
        }
    }
    for (const auto& a : first->getNodes()) {
        for (const auto& b : second->getNodes()) {
            REQUIRE(a != b);
        }
    }
}

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
