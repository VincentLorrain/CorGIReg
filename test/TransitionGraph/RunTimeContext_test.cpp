#include <catch2/catch_test_macros.hpp>
#include "BasicGraphIr/Graph.hpp"
#include "TransitionGraph/State.hpp"
#include "TransitionGraph/RunTimeContext.hpp"
#include "TransitionGraph/Transition.hpp"

#include <memory>

using namespace CorGIReg;

TEST_CASE("RunTimeContext: Basic Functionality") {

    SECTION("create a new RunTimeContext") {

        auto stateA = std::make_shared<State>(true,false);
        REQUIRE(stateA->isValid() == true);
        REQUIRE(stateA->isStart() == false);
        REQUIRE(stateA->getTransitions().size() == 0);

        auto stateB = std::make_shared<State>(false,true);
        
        REQUIRE(stateB->isValid() == false);
        REQUIRE(stateB->isStart() == true);
        REQUIRE(stateB->getTransitions().size() == 0);

        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return true; });

        auto tr =  TransitionFactory::make(stateA,stateB,TransitionTypes::UNIQUE,"A");

        REQUIRE(stateA->getTransitions().size() == 1);
        REQUIRE(stateB->getParentStates().size() == 1);

    }
}