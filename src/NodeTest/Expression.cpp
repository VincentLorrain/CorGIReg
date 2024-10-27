#include "NodeTest/Expression.hpp"

namespace CorGIReg {
    

ValueExpression::ValueExpression(std::function<bool(std::shared_ptr<NodeNN>)> f)
    : func(f) {}

bool ValueExpression::evaluate(std::shared_ptr<NodeNN> node) {
    return func(node);
}

BinaryExpression::BinaryExpression(Operator op, std::shared_ptr<Expression> left, std::shared_ptr<Expression> right)
    : op(op), left(left), right(right) {}

bool BinaryExpression::evaluate(std::shared_ptr<NodeNN> node) {
    if (op == AND) {
        return left->evaluate(node) && right->evaluate(node);
    } else if (op == OR) {
        return left->evaluate(node) || right->evaluate(node);
    } else {
        throw std::runtime_error("Invalid operator");
    }
}

}