
#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "Grexgex/RandomExpressionGenerator.h"
#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;


TEST_CASE("PartitionGraphParser", "Test_GraphParser") {

    SECTION("Correct parsing") {
        RandomExpressionGenerator::clearDictionary();
        RandomExpressionGenerator::setSeed(20261026);
        for (int i = 0; i < 1000; ++i) {
            const std::string test = RandomExpressionGenerator::allExpr();
            INFO("seed=20261026, case=" << i << ", query=" << test);
            PartitionGraphParser graphParser = PartitionGraphParser(test);
            std::shared_ptr<AstNode<tGTokensTypes>> tree = graphParser.parse();
            REQUIRE(tree != nullptr);
            REQUIRE(graphParser.parse()->getType() == tree->getType());
        }
    }

    SECTION("Throws END token") {
        REQUIRE_THROWS(PartitionGraphParser("A->$->B").parse());
        REQUIRE_THROWS(PartitionGraphParser("$->B").parse());
        REQUIRE_THROWS(PartitionGraphParser("$#").parse());
        REQUIRE_THROWS(PartitionGraphParser("$").parse());
        REQUIRE_THROWS(PartitionGraphParser("A#$").parse());
    }

    SECTION("Throws PREC token") {
        REQUIRE_THROWS(PartitionGraphParser("<-A").parse());
        REQUIRE_THROWS(PartitionGraphParser("A<-B->C").parse());
        REQUIRE_THROWS(PartitionGraphParser("A<-$").parse());
    }
}
TEST_CASE("Parser refuses boundaries that cannot hold", "[parser][boundary]") {
    for (const std::string query : {"A->$", "$<-B<-A", "A->B?->$", "(A->$)?", "A#->$;A#->B?", "(A->D#->$)+",
                                    "(A+->B)+->$", "A*->B+->C?->$", "A#->$;B->A#", "(A#->B)?;A#->$"}) {
        INFO(query);
        REQUIRE_NOTHROW(PartitionGraphParser(query).parse());
    }
    // Repeated group without anchor, nothing guaranteed before '$', anchor
    // ending at '$' with a mandatory successor.
    for (const std::string query : {"(A->$)+", "(A->$)*", "(B->A->$)+->C", "B?->$", "A;B*->$", "(A?)?->$",
                                    "$<-B?", "A#->$;A#->B", "(A#->B)+;A#->$", "X#->(A?)?->$;X#->Y", "B<-A#;$<-A#"}) {
        INFO(query);
        try { PartitionGraphParser(query).parse(); FAIL("expected an illegal boundary"); }
        catch (const std::invalid_argument& error) {
            REQUIRE(std::string(error.what()).rfind("Illegal boundary: ", 0) == 0);
        }
    }
}
