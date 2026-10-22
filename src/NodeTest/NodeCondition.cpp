#include "NodeTest/NodeCondition.hpp"
#include "NodeTest/FunctionBuilder.hpp"

namespace CorGIReg{

NodeCondition::NodeCondition(const std::string key,const std::string ConditionalExpressions)
    : mKey(key), mTest(buildFunctionFromString(ConditionalExpressions)) {}

NodeCondition::NodeCondition(const std::string key, const NodeValue& rule) : mKey(key) {
    auto compiled = compileNodeRule(rule);
    mTest = [compiled](std::shared_ptr<NodeNN> node) { return compiled(nodeDocument(*node)); };
}

}
