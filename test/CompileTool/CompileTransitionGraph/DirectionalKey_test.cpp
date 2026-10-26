#include <catch2/catch_test_macros.hpp>
#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"
using namespace CorGIReg;

TEST_CASE("Directional common keys preserve lexical sides and numbered identity", "[directional][parser]") {
    for (const std::string text : {"$B", "B$", "$B$", "$B#01", "B$1", "$B$01"}) {
        INFO(text);
        PartitionGraphLexer lexer(text + "->$");
        auto key = lexer.getNextToken();
        REQUIRE(key->getType() == tGTokensTypes::SCKEY);
        REQUIRE(key->getLexeme() == text);
        REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::NEXT);
        REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::END);
        REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::STOP);
        const auto decoded = parseCommonKey(text);
        REQUIRE(decoded.predicate == "B");
        REQUIRE(decoded.strictInputs == (text.front() == '$'));
        REQUIRE(decoded.strictOutputs == (text.find('$', 1) != std::string::npos));
    }
    REQUIRE(parseCommonKey("$B#01").identity() == parseCommonKey("$B$1").identity());
    REQUIRE(parseCommonKey("B#1").identity() == parseCommonKey("B$01").identity());
    REQUIRE(parseCommonKey("$B").identity() != parseCommonKey("B#0").identity());
    REQUIRE(parseCommonKey("$B123").predicate == "B123");
    for (const auto* text : {"$$B", "B$$", "$B$1x", "$", "$B+", "B#x", "A@->B"}) {
        INFO(text);
        REQUIRE_THROWS(PartitionGraphParser(text).parse());
    }
    PartitionGraphParser parser("$B$<-A;($B#01->C)+");
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        auto ast = parser.parse();
        REQUIRE(ast->getType() == tGTokensTypes::SEP);
        REQUIRE(ast->getChilds()[0]->getType() == tGTokensTypes::PREC);
        REQUIRE(ast->getChilds()[0]->getChilds()[0]->getValue() == "$B$");
    }
}

TEST_CASE("Directional common identities agree in root planning and generated IR", "[directional][ir]") {
    const std::string query = "A->$B#01;B$1->C;$B$1->D";
    auto plan = RootSelectionPass{}.run(PartitionGraphParser(query).parse());
    REQUIRE(plan.components.size() == 1);
    REQUIRE(plan.nodes.size() == 4);
    REQUIRE(plan.nodes[*plan.components[0].root].occurrences.size() == 3);
    PGInterpreterIR generator(query, 0);
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        auto graph = generator.interpret();
        REQUIRE(graph->getNodes().size() == 4);
        unsigned anchors = 0;
        for (const auto& node : graph->getNodes()) {
            if (node->getType() != "B") continue;
            ++anchors;
            REQUIRE(node->getParents().size() == 1);
            REQUIRE(node->getChildren().size() == 2);
        }
        REQUIRE(anchors == 1);
    }
}
