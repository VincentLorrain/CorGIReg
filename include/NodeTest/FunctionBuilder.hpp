#pragma once

#include <memory>
#include <functional>
#include <string>
#include "NodeTest/ExpressionParser.hpp"

namespace CorGIReg {
std::function<bool(std::shared_ptr<NodeNN>)> buildFunctionFromString(const std::string& expressionStr);
}
    

