#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeCondition.hpp"
#include "NodeTest/NodeDictionary.hpp"

using namespace CorGIReg;
namespace {
using V = NodeValue;
using O = V::Object;
using A = V::Array;
V field(A path, const std::string& op, V value) { return O{{"path", path}, {op, value}}; }
}

TEST_CASE("Node rules distinguish presence null false and wrong types", "[NodeTest][rules]") {
    V doc = O{{"attributes", O{{"nil", nullptr}, {"zero", 0}, {"flag", false}, {"empty", ""}}}};
    for (const auto& key : {"nil", "zero", "flag", "empty"})
        REQUIRE(compileNodeRule(field({"attributes", key}, "exists", true))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "missing"}, "exists", false))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "missing"}, "ne", 3))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "empty"}, "ne", 3))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "flag"}, "eq", 0))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "nil"}, "eq", nullptr))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "zero"}, "eq", 0.0))(doc));
}

TEST_CASE("Node rules compare typed values arrays and regexes", "[NodeTest][rules]") {
    V doc = O{{"name", "encoder/conv_12"}, {"attributes", O{{"kernel", A{3, 3}}, {"group", 2}}},
              {"inputs", A{"x", "w"}}};
    REQUIRE(compileNodeRule(field({"attributes", "kernel"}, "eq", A{3, 3}))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "kernel"}, "eq", A{1, 1}))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "kernel"}, "contains", 3))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "group"}, "in", A{1, 2}))(doc));
    REQUIRE(compileNodeRule(field({"inputs"}, "length", O{{"eq", 2}}))(doc));
    REQUIRE(compileNodeRule(field({"inputs", 1}, "eq", "w"))(doc));
    REQUIRE(compileNodeRule(field({"inputs", 2}, "exists", false))(doc));
    V optionalInputs = O{{"inputs", A{"x", V::Missing{}, nullptr, ""}}};
    REQUIRE(compileNodeRule(field({"inputs", 1}, "exists", false))(optionalInputs));
    REQUIRE(compileNodeRule(field({"inputs", 2}, "exists", true))(optionalInputs));
    REQUIRE(compileNodeRule(field({"inputs", 3}, "exists", true))(optionalInputs));
    REQUIRE(compileNodeRule(field({"name"}, "regex", "encoder/conv_[0-9]+"))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"name"}, "regex", "conv_[0-9]+"))(doc));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "group"}, "regex", ".*"))(doc));
    for (const auto& op : {"gte", "lte"}) REQUIRE(compileNodeRule(field({"attributes", "group"}, op, 2))(doc));
    for (const auto& op : {"gt", "lt"}) REQUIRE_FALSE(compileNodeRule(field({"attributes", "group"}, op, 2))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "group"}, "gt", 1))(doc));
    REQUIRE(compileNodeRule(field({"attributes", "group"}, "lt", 3))(doc));
}

TEST_CASE("Node rules compose and count matching keys", "[NodeTest][rules]") {
    V clean = O{{"attributes", O{{"kernel.size", 3}}}};
    V debug = O{{"attributes", O{{"kernel.size", 3}, {"debug_trace", false}}}};
    V keys = O{{"keys", A{"attributes"}}, {"regex", "debug_.*"}, {"count", O{{"eq", 0}}}};
    REQUIRE(compileNodeRule(keys)(clean));
    REQUIRE_FALSE(compileNodeRule(keys)(debug));
    REQUIRE_FALSE(compileNodeRule(keys)(O{}));
    V present = field({"attributes", "kernel.size"}, "eq", 3);
    REQUIRE(compileNodeRule(O{{"all", A{present, keys}}})(clean));
    REQUIRE_FALSE(compileNodeRule(O{{"all", A{present, keys}}})(debug));
    REQUIRE(compileNodeRule(O{{"any", A{keys, present}}})(debug));
    REQUIRE(compileNodeRule(O{{"not", keys}})(debug));
    // Every branch is compiled even if an earlier branch would succeed at runtime.
    REQUIRE_THROWS_AS(compileNodeRule(O{{"any", A{present, O{{"bad", true}}}}}), std::invalid_argument);
}

TEST_CASE("Node rules reject malformed definitions before evaluation", "[NodeTest][rules]") {
    for (const auto& rule : A{O{}, O{{"all", A{}}}, field({-1}, "eq", 0),
            field({"x"}, "regex", "["), field({"x"}, "exists", 1),
            field({"x"}, "gt", "1"), field({"x"}, "in", 1),
            field({"x"}, "length", O{{"regex", ".*"}}),
            O{{"path", A{"x"}}, {"eq", 1}, {"ne", 2}}}) {
        REQUIRE_THROWS_AS(compileNodeRule(rule), std::invalid_argument);
    }
}

TEST_CASE("NodeCondition evaluates legacy expressions and declarative NodeNN rules", "[NodeTest][rules]") {
    auto node = std::make_shared<NodeNN>();
    node->setInfo("Type", std::string("Conv"));
    node->setInfo("kernel", std::vector<int>{3, 3});
    node->setInfo("bias", false);
    node->setInfo("inputs", std::vector<std::string>{"x", "w", ""});
    REQUIRE(compileNodeRule(field({"inputs", 2}, "exists", false))(nodeDocument(*node)));
    node->setInfo("custom", std::make_shared<int>(2));
    NodeCondition legacy("LegacyRuleTest", "getType() == Conv && hasArgument(kernel)");
    REQUIRE(legacy.test(node));
    REQUIRE_FALSE(legacy.test(std::make_shared<NodeNN>()));
    REQUIRE_FALSE(legacy.test(nullptr));
    REQUIRE_THROWS(NodeCondition("InvalidRuleTest", "unknown()"));
    NodeCondition condition("KernelRuleTest", O{{"all", A{
        field({"attributes", "kernel"}, "eq", A{3, 3}),
        field({"attributes", "bias"}, "exists", true)}}});
    REQUIRE(condition.test(node));
    node->setInfo("kernel", std::string("3,3"));
    REQUIRE_FALSE(condition.test(node));
    REQUIRE(compileNodeRule(field({"attributes", "custom"}, "exists", true))(nodeDocument(*node)));
    REQUIRE_FALSE(compileNodeRule(field({"attributes", "custom"}, "ne", 0))(nodeDocument(*node)));
    REQUIRE(NodeDictionary::registerFunction("DeclarativeKernelRuleTest", [condition](const NodeNN::Ptr& n) { return condition.test(n); }));
    REQUIRE_FALSE(NodeDictionary::at("DeclarativeKernelRuleTest")(node));
}
