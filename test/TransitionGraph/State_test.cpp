#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "TransitionGraph/State.hpp"
#include "NodeTest/NodeDictionary.hpp"
#include "TransitionGraph/Transition.hpp"



using namespace CorGIReg;

TEST_CASE("State: Basic Functionality") {

    SECTION("State and transition") {

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
        REQUIRE(tr->getSourceState() == stateA);
        REQUIRE(tr->getDestState() == stateB);


        REQUIRE(stateA->getTransitions().size() == 1);
        REQUIRE(stateB->getParentStates().size() == 1);

        std::weak_ptr<Transition> trFromStateA = *(stateA->getTransitions()).begin();
        REQUIRE(trFromStateA.lock() == tr);

        std::weak_ptr<CorGIReg::State> parent = *(stateB->getParentStates()).begin();
        REQUIRE(parent.lock() == stateA);

    }
}