#include <catch2/catch_test_macros.hpp>
#include "CompileTool/Common/NodePattern.hpp"
#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"

using namespace CorGIReg;

TEST_CASE("Inline node lexer preserves JSON values and anchor markers", "[inline-node][lexer]") {
    PartitionGraphLexer lexer(R"($conv(kernel_shape=[3,3],label="a),->b")$2+->Relu)");
    const auto key = lexer.getNextToken();
    REQUIRE(key->getType() == tGTokensTypes::SCKEY);
    REQUIRE(key->getLexeme() == R"($conv(kernel_shape=[3,3],label="a),->b")$2)");
    REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::QOM);
    REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::NEXT);
    REQUIRE(lexer.getNextToken()->getLexeme() == "Relu");
    REQUIRE(lexer.getNextToken()->getType() == tGTokensTypes::STOP);
    const auto node = parseNodePattern(key->getLexeme());
    REQUIRE(node.name == "conv");
    REQUIRE(node.conditions.size() == 2);
    REQUIRE(std::get<NodeValue::Array>(node.conditions[0].second.data).size() == 2);
    REQUIRE(std::get<std::string>(node.conditions[1].second.data) == "a),->b");
    REQUIRE(parseCommonKey(key->getLexeme()).identity() == "conv#2");
    REQUIRE(parseCommonKey(key->getLexeme()).strictInputs);
    REQUIRE(parseCommonKey(key->getLexeme()).strictOutputs);
}

TEST_CASE("Inline filters are node leaves under groups arrows and quantifiers", "[inline-node][parser]") {
    PartitionGraphParser parser("(conv(kernel_shape=[3,3])->Relu)+");
    const auto tree = parser.parse();
    REQUIRE(tree->getType() == tGTokensTypes::QOM);
    const auto sequence = tree->getChilds().at(0)->getChilds().at(0);
    REQUIRE(sequence->getType() == tGTokensTypes::NEXT);
    REQUIRE(sequence->getChilds().at(0)->getType() == tGTokensTypes::KEY);
    REQUIRE(sequence->getChilds().at(0)->getValue() == "conv(kernel_shape=[3,3])");
    REQUIRE(parser.parse()->getType() == tree->getType());
    auto pattern = PGInterpreterTG("conv(kernel_shape=[3,3])+->Relu").interpret();
    std::set<std::string> keys;
    for (const auto& transition : pattern->getTransitions()) keys.insert(transition->getTestKey());
    REQUIRE(keys.count("conv(kernel_shape=[3,3])") == 1);
    REQUIRE(keys.count("Relu") == 1);
    // Generation must reject even a filter inside a group that would be skipped.
    REQUIRE_THROWS_AS(PGInterpreterIR("conv(kernel_shape=[3,3])*", 0).interpret(), std::invalid_argument);
}

TEST_CASE("Inline node conditions reject malformed values and shorthand", "[inline-node][parser]") {
    for (const auto* query : {"conv(kernel_shape=3x3)", "conv(mode=SAME)", "conv(kernel_shape=)",
        "conv(kernel_shape=[3,3)", "conv(kernel_shape=[3,3],)", "conv(kernel_shape=3,kernel_shape=5)",
        "conv(kernel_shape==3)", "conv(kernel_shape=3", "conv()", "conv(kernel_shape)"}) {
        INFO(query);
        REQUIRE_THROWS(PartitionGraphParser(query).parse());
    }
    const auto node = parseNodePattern(R"(conv(n=-2,ratio=0.5,enabled=true,optional=null,text="SAME"))");
    REQUIRE(node.conditions.size() == 5);
    REQUIRE(std::get<std::int64_t>(node.conditions[0].second.data) == -2);
    REQUIRE(std::get<bool>(node.conditions[2].second.data));
}
