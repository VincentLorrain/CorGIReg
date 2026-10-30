#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <memory>
#include <set>
#include <thread>
#include <type_traits>
#include <vector>

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
TEST_CASE("State: identity cannot be duplicated", "[state]") {

    SECTION("States are not copyable") {
        STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<State>);
        STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<State>);
    }

    SECTION("Ids increase in creation order") {
        auto first = std::make_shared<State>(false, true);
        auto second = std::make_shared<State>(false, true);
        REQUIRE(first->getId() < second->getId());
    }

    SECTION("States built on several threads get distinct ids") {
        constexpr std::size_t threads = 8, perThread = 1000;
        std::vector<std::vector<std::size_t>> ids(threads);
        std::vector<std::thread> workers;
        for (std::size_t t = 0; t < threads; ++t)
            workers.emplace_back([&ids, t] {
                for (std::size_t i = 0; i < perThread; ++i) ids[t].push_back(std::make_shared<State>(false, false)->getId());
            });
        for (auto& worker : workers) worker.join();
        std::set<std::size_t> all;
        for (const auto& local : ids) {
            REQUIRE(std::is_sorted(local.begin(), local.end()));
            all.insert(local.begin(), local.end());
        }
        REQUIRE(all.size() == threads * perThread);
    }
}
