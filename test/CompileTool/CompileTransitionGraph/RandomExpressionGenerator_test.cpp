#include <catch2/catch_test_macros.hpp>
#include "Grexgex/RandomExpressionGenerator.h"
#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include <array>
using namespace CorGIReg;

TEST_CASE("Seeded expression generator covers directional keys with bounded nesting", "[directional][random]") {
    struct Reset { ~Reset() { RandomExpressionGenerator::clearDictionary(); } } reset;
    RandomExpressionGenerator::setDictionary({"SeedA", "SeedB", "SeedC"});
    std::array<unsigned, 4> seen{};
    unsigned groups = 0, boundaries = 0, numbered = 0;
    for (unsigned seed : {20261026u, 17u, 91u}) {
        RandomExpressionGenerator::setSeed(seed);
        std::vector<std::string> queries;
        for (unsigned trial = 0; trial < 80; ++trial) {
            const auto query = RandomExpressionGenerator::allExpr(2);
            INFO("seed=" << seed << ", case=" << trial << ", query=" << query);
            queries.push_back(query);
            auto ast = PartitionGraphParser(query).parse();
            std::function<void(const std::shared_ptr<AstNode<tGTokensTypes>>&, unsigned)> visit;
            visit = [&](const auto& node, unsigned depth) {
                if (node->getType() == tGTokensTypes::LPAREN) { ++depth; ++groups; }
                REQUIRE(depth <= 2);
                if (node->getType() == tGTokensTypes::END) ++boundaries;
                if (node->getType() == tGTokensTypes::CKEY || node->getType() == tGTokensTypes::SCKEY) {
                    const auto key = parseCommonKey(node->getValue());
                    ++seen[(key.strictInputs ? 1 : 0) + (key.strictOutputs ? 2 : 0)];
                    numbered += !key.suffix.empty();
                }
                for (const auto& child : node->getChilds()) visit(child, depth);
            };
            visit(ast, 0);
            REQUIRE_FALSE(PGInterpreterTG(query).interpret()->getTransitions().empty());
            REQUIRE_THROWS(PartitionGraphParser(query + "@").parse());
        }
        RandomExpressionGenerator::setSeed(seed);
        for (const auto& query : queries) REQUIRE(RandomExpressionGenerator::allExpr(2) == query);
    }
    for (auto count : seen) REQUIRE(count > 0);
    REQUIRE(groups > 0); REQUIRE(boundaries > 0); REQUIRE(numbered > 0);
    RandomExpressionGenerator::setSeed(0);
    for (unsigned i = 0; i < 100; ++i) REQUIRE(RandomExpressionGenerator::allExpr(0).find('(') == std::string::npos);
    REQUIRE_THROWS_AS(RandomExpressionGenerator::allExpr(7), std::invalid_argument);
    REQUIRE_THROWS_AS(RandomExpressionGenerator::setDictionary({""}), std::invalid_argument);
    REQUIRE_THROWS_AS(RandomExpressionGenerator::setDictionary({"A$"}), std::invalid_argument);
}
