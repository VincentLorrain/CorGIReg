
#pragma once

#include <memory>
#include <functional>
#include <stdexcept>
#include "BasicGraphIr/Graph.hpp"

namespace CorGIReg{

class Expression {
public:
    virtual bool evaluate(std::shared_ptr<NodeNN> node) = 0;
    virtual ~Expression() {}
};

class ValueExpression : public Expression {
private:
    std::function<bool(std::shared_ptr<NodeNN>)> func;
public:
    ValueExpression(std::function<bool(std::shared_ptr<NodeNN>)> f);
    bool evaluate(std::shared_ptr<NodeNN> node) override;
};

class BinaryExpression : public Expression {
public:
    enum Operator {
        AND,
        OR
    };
private:
    Operator op;
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
public:
    BinaryExpression(Operator op, std::shared_ptr<Expression> left, std::shared_ptr<Expression> right);
    bool evaluate(std::shared_ptr<NodeNN> node) override;
};

}