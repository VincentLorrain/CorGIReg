
#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "Grexgex/RandomExpressionGenerator.h"
#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;


TEST_CASE("PartitionGraphParser", "Test_GraphParser") {

    SECTION("Correct parsing") {
        for (int i = 0; i < 1000; ++i) {
            const std::string test = RandomExpressionGenerator::allExpr();
            //std::cout << test <<"\n";
            PartitionGraphParser graphParser = PartitionGraphParser(test);
            std::shared_ptr<AstNode<tGTokensTypes>> tree = graphParser.parse();
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