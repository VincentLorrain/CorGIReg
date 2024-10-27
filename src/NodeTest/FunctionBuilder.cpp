#include "NodeTest/FunctionBuilder.hpp"

namespace CorGIReg {
    
std::function<bool(std::shared_ptr<NodeNN>)> buildFunctionFromString(const std::string& expressionStr) {
    ExpressionParser parser(expressionStr);
    auto expr = parser.parse();
    return [expr](std::shared_ptr<NodeNN> node) -> bool {
        return expr->evaluate(node);
    };
}

}