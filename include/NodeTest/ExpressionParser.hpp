#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "NodeTest/Expression.hpp"


namespace CorGIReg {


class ExpressionParser {
private:
    std::string input;
    size_t mPos;

    void skipWhitespace();
    std::shared_ptr<Expression> parsePrimary();
    std::shared_ptr<Expression> parseFunctionCall();
    std::string parseIdentifier();
    std::string parseStringLiteral();
    std::shared_ptr<Expression> parseAndExpression();
    std::shared_ptr<Expression> parseExpression();
    std::shared_ptr<Expression> createFunctionExpression(const std::string& funcName, const std::string& arg);
    std::shared_ptr<Expression> createEqualityExpression(const std::string& funcName, const std::string& arg, const std::string& value, bool isEqual);

public:
    ExpressionParser(const std::string& expr);
    std::shared_ptr<Expression> parse();
};


}