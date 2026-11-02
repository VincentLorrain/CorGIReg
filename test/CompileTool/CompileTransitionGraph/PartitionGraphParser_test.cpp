
#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "Generation/RandomExpressionGenerator.h"
#include <iostream>
#include <map>
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

namespace {
using AltAst = std::shared_ptr<AstNode<tGTokensTypes>>;
// Compact syntax tree: type name, value, children.
std::string shape(const AltAst& node) {
    static const std::map<tGTokensTypes, std::string> names{
        {tGTokensTypes::NEXT, "NEXT"}, {tGTokensTypes::PREC, "PREC"}, {tGTokensTypes::QOM, "+"},
        {tGTokensTypes::QZM, "*"}, {tGTokensTypes::QZO, "?"}, {tGTokensTypes::KEY, "KEY"},
        {tGTokensTypes::CKEY, "CKEY"}, {tGTokensTypes::SCKEY, "SCKEY"}, {tGTokensTypes::SEP, "SEP"},
        {tGTokensTypes::LPAREN, "GROUP"}, {tGTokensTypes::END, "END"}, {tGTokensTypes::ALT, "ALT"}};
    std::string out = names.at(node->getType());
    if (node->getType() == tGTokensTypes::KEY) out += ":" + node->getValue();
    if (node->getChilds().empty()) return out;
    out += "(";
    for (std::size_t i = 0; i < node->getChilds().size(); ++i) out += (i ? "," : "") + shape(node->getChilds()[i]);
    return out + ")";
}
std::string parseError(const std::string& query) {
    try { PartitionGraphParser(query).parse(); }
    catch (const std::exception& error) { return error.what(); }
    FAIL("expected a parse error for " << query);
    return {};
}
}

TEST_CASE("Parser builds alternation groups", "[parser][alternation]") {
    // Branches are full sequences, in lexical order, under an n-ary ALT node.
    REQUIRE(shape(PartitionGraphParser("X->(A->B|C)->Y").parse()) ==
            "NEXT(KEY:X,NEXT(GROUP(ALT(NEXT(KEY:A,KEY:B),KEY:C)),KEY:Y))");
    REQUIRE(shape(PartitionGraphParser("(A|B|C)").parse()) == "GROUP(ALT(KEY:A,KEY:B,KEY:C))");
    // The quantifier is optional for an alternation and applies to the whole group.
    REQUIRE(shape(PartitionGraphParser("(A|B)+").parse()) == "+(GROUP(ALT(KEY:A,KEY:B)))");
    REQUIRE(shape(PartitionGraphParser("((A|B)->C|D)*").parse()) ==
            "*(GROUP(ALT(NEXT(GROUP(ALT(KEY:A,KEY:B)),KEY:C),KEY:D)))");
    REQUIRE(shape(PartitionGraphParser("Y<-(A|B?)").parse()) == "PREC(KEY:Y,GROUP(ALT(KEY:A,?(KEY:B))))");
    // A plain group still requires a quantifier.
    REQUIRE(parseError("(A)").find("must have quantifier") != std::string::npos);
    REQUIRE(parseError("(A->B)->C").find("must have quantifier") != std::string::npos);
}

TEST_CASE("Parser refuses alternation forms outside the first version", "[parser][alternation]") {
    for (const std::string query : {"A|B", "A->B|C", "(A)+|B", "A;B|C"}) {
        INFO(query);
        REQUIRE(parseError(query).find("Alternation requires parentheses") != std::string::npos);
    }
    for (const std::string query : {"(A|)", "(|A)", "(A||B)", "()+"}) {
        INFO(query);
        REQUIRE(parseError(query).find("Empty group or alternative") != std::string::npos);
    }
    // Anchors and boundaries are refused anywhere below an alternation, also
    // through nested groups, but stay legal next to it.
    for (const std::string query : {"(A#|B)", "($A|B)", "(A$|B)", "(B|$A$1)", "((A#)+|B)", "(X->(A#|B)+|C)"}) {
        INFO(query);
        try { PartitionGraphParser(query).parse(); FAIL("expected an illegal alternation"); }
        catch (const std::invalid_argument& error) {
            REQUIRE(std::string(error.what()) == "Illegal alternation: anchors are not supported inside alternatives, as in (A#|B)");
        }
    }
    for (const std::string query : {"(A->$|B)", "(B|$<-A)", "((A->$)?|B)"}) {
        INFO(query);
        try { PartitionGraphParser(query).parse(); FAIL("expected an illegal alternation"); }
        catch (const std::invalid_argument& error) {
            REQUIRE(std::string(error.what()) == "Illegal alternation: '$' is not supported inside alternatives, as in (A->$|B)");
        }
    }
    for (const std::string query : {"X#->(A|B)->Y;X#->Z", "(A|B)->$", "X->(A|B?)->$", "(A|B)->C#;C#->D"}) {
        INFO(query);
        REQUIRE_NOTHROW(PartitionGraphParser(query).parse());
    }
}

TEST_CASE("Boundary rules see through alternation groups", "[parser][alternation][boundary]") {
    // A group is guaranteed before '$' only when no alternative can be empty.
    REQUIRE_NOTHROW(PartitionGraphParser("(A|B->C)->$").parse());
    for (const std::string query : {"(A|B?)->$", "(A?|B*)->$", "(A|B)?->$"}) {
        INFO(query);
        try { PartitionGraphParser(query).parse(); FAIL("expected an illegal boundary"); }
        catch (const std::invalid_argument& error) {
            REQUIRE(std::string(error.what()).rfind("Illegal boundary: no operation is guaranteed", 0) == 0);
        }
    }
}
