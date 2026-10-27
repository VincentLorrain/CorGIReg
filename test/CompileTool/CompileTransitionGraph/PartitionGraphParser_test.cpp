
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