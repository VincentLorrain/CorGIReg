#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "TransitionGraph/State.hpp"
#include "TransitionGraph/Transition.hpp"
#include "TransitionGraph/TransitionPort.hpp"
#include "TransitionGraph/TransitionTrace.hpp"

using namespace CorGIReg;

TEST_CASE("Transition: getType reports the built kind") {

    auto source = std::make_shared<State>(false, true);
    auto dest = std::make_shared<State>(true, false);

    SECTION("Factory kinds round-trip") {
        REQUIRE(TransitionFactory::make(source, dest, TransitionTypes::EMPTY)->getType() == TransitionTypes::EMPTY);
        REQUIRE(TransitionFactory::make(source, dest, TransitionTypes::UNIQUE, "TypeTestUnique")->getType() == TransitionTypes::UNIQUE);
        REQUIRE(TransitionFactory::make(source, dest, TransitionTypes::NNULL)->getType() == TransitionTypes::NNULL);
    }

    SECTION("Common kind follows strictness of the spelling") {
        auto common = TransitionFactory::make(source, dest, TransitionTypes::COMMON, "TypeTestCommon#");
        REQUIRE(common->getType() == TransitionTypes::COMMON);
        REQUIRE(common->isCommon());

        for (const auto* lexeme : {"$TypeTestStrict#1", "TypeTestStrict$1", "$TypeTestStrict$1"}) {
            auto strict = TransitionFactory::make(source, dest, TransitionTypes::STRICTCOMMON, lexeme);
            REQUIRE(strict->getType() == TransitionTypes::STRICTCOMMON);
            REQUIRE(strict->isCommon());
        }
    }

    SECTION("Reject-all transition has its own kind") {
        auto none = std::make_shared<TransitionNone>(source, dest);
        REQUIRE(none->getType() == TransitionTypes::NONE);
        REQUIRE_THROWS_AS(TransitionFactory::make(source, dest, TransitionTypes::NONE), std::invalid_argument);
    }

    SECTION("Port and trace actions are empty transitions") {
        auto port = std::make_shared<TransitionPort>(source, dest, PortAction::Check, 0);
        auto trace = std::make_shared<TransitionTrace>(source, dest, TraceAction::Begin, 0, '+');
        REQUIRE(port->getType() == TransitionTypes::EMPTY);
        REQUIRE(trace->getType() == TransitionTypes::EMPTY);
        REQUIRE_FALSE(port->isCommon());
        REQUIRE_FALSE(trace->isCommon());
    }
}
