#include <catch2/catch_test_macros.hpp>
#include "BasicGraphIr/Graph.hpp"
#include "TransitionGraph/TransitionGraph.hpp"

#include <memory>

using namespace CorGIReg;

TEST_CASE("TransitionGraph: Basic Functionality") {

    SECTION("create a new TransitionGraph") {

        //make the symbol dictionary
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return true; });
        REQUIRE(NodeDictionary::hasFunction("A") );
        
        auto tGA1 = std::make_shared<TransitionGraph>(TransitionTypes::UNIQUE,"A","A->A",0);
        auto tGA2 = std::make_shared<TransitionGraph>(TransitionTypes::UNIQUE,"A","A->A",0);
        
        auto compiled = tGA1->mergeOneStartOneValid(tGA2)->compile();
        auto transitions = compiled->getTransitions();
        REQUIRE(transitions.size() == 2);
        REQUIRE(compiled->getAllOrigin() == std::set<std::size_t>({0}));

        for(auto transition : transitions) {
            REQUIRE(std::dynamic_pointer_cast<TransitionUnique>(transition) != nullptr);
        }

    }
}