#include "NodeTest/ExpressionParser.hpp"
#include <cctype>
#include <stdexcept>


namespace CorGIReg {
    

ExpressionParser::ExpressionParser(const std::string& expr) : input(expr), mPos(0) {}

void ExpressionParser::skipWhitespace() {
    while (mPos < input.length() && isspace(input[mPos])) {
        mPos++;
    }
}

std::shared_ptr<Expression> ExpressionParser::parsePrimary() {
    skipWhitespace();
    if (mPos >= input.length()) {
        throw std::runtime_error("Unexpected end of expression");
    }

    if (input[mPos] == '(') {
        mPos++; // Consume '('
        auto expr = parseExpression();
        skipWhitespace();
        if (mPos >= input.length() || input[mPos] != ')') {
            throw std::runtime_error("Expected ')'");
        }
        mPos++; // Consume ')'
        return expr;
    } else {
        return parseFunctionCall();
    }
}

std::shared_ptr<Expression> ExpressionParser::parseFunctionCall() {
    skipWhitespace();
    std::string funcName = parseIdentifier();
    skipWhitespace();
    if (mPos >= input.length() || input[mPos] != '(') {
        throw std::runtime_error("Expected '(' after function name");
    }
    mPos++; // Consume '('
    skipWhitespace();

    std::string arg;
    if (mPos < input.length() && input[mPos] != ')') {
        // Parse argument (identifier or string literal)
        if (input[mPos] == '"') {
            arg = parseStringLiteral();
        } else {
            arg = parseIdentifier();
        }
    }

    skipWhitespace();
    if (mPos >= input.length() || input[mPos] != ')') {
        throw std::runtime_error("Expected ')'");
    }
    mPos++; // Consume ')'
    skipWhitespace();

    // Check for comparison operator
    if (mPos + 1 < input.length() && input[mPos] == '=' && input[mPos + 1] == '=') {
        mPos += 2; // Consume '=='
        skipWhitespace();
        std::string value;
        if (input[mPos] == '"') {
            value = parseStringLiteral();
        } else {
            value = parseIdentifier();
        }
        return createEqualityExpression(funcName, arg, value, true);
    } else if (mPos + 1 < input.length() && input[mPos] == '!' && input[mPos + 1] == '=') {
        mPos += 2; // Consume '!='
        skipWhitespace();
        std::string value;
        if (input[mPos] == '"') {
            value = parseStringLiteral();
        } else {
            value = parseIdentifier();
        }
        return createEqualityExpression(funcName, arg, value, false);
    } else {
        // Assume function returns a bool
        return createFunctionExpression(funcName, arg);
    }
}

std::string ExpressionParser::parseIdentifier() {
    skipWhitespace();
    size_t start = mPos;
    while (mPos < input.length() && (isalnum(input[mPos]) || input[mPos] == '_')) {
        mPos++;
    }
    if (start == mPos) {
        throw std::runtime_error("Expected identifier at position " + std::to_string(mPos));
    }
    return input.substr(start, mPos - start);
}

std::string ExpressionParser::parseStringLiteral() {
    skipWhitespace();
    if (input[mPos] != '"') {
        throw std::runtime_error("Expected '\"' at position " + std::to_string(mPos));
    }
    mPos++; // Consume '"'
    size_t start = mPos;
    while (mPos < input.length() && input[mPos] != '"') {
        mPos++;
    }
    if (mPos >= input.length()) {
        throw std::runtime_error("Unterminated string literal");
    }
    std::string str = input.substr(start, mPos - start);
    mPos++; // Consume closing '"'
    return str;
}

std::shared_ptr<Expression> ExpressionParser::parseAndExpression() {
    auto left = parsePrimary();
    while (true) {
        skipWhitespace();
        if (mPos + 1 < input.length() && input[mPos] == '&' && input[mPos + 1] == '&') {
            mPos += 2; // Consume '&&'
            auto right = parsePrimary();
            left = std::make_shared<BinaryExpression>(BinaryExpression::AND, left, right);
        } else {
            break;
        }
    }
    return left;
}

std::shared_ptr<Expression> ExpressionParser::parseExpression() {
    auto left = parseAndExpression();
    while (true) {
        skipWhitespace();
        if (mPos + 1 < input.length() && input[mPos] == '|' && input[mPos + 1] == '|') {
            mPos += 2; // Consume '||'
            auto right = parseAndExpression();
            left = std::make_shared<BinaryExpression>(BinaryExpression::OR, left, right);
        } else {
            break;
        }
    }
    return left;
}

std::shared_ptr<Expression> ExpressionParser::createFunctionExpression(const std::string& funcName, const std::string& arg) {
    if (funcName == "hasArgument") {
        return std::make_shared<ValueExpression>([arg](std::shared_ptr<NodeNN> node) -> bool {
            return node->hasArgument(arg);
        });
    } else if (funcName == "getType") {
        return std::make_shared<ValueExpression>([arg](std::shared_ptr<NodeNN> node) -> bool {
            return node->getType() == arg;
        });
    } else {
        throw std::runtime_error("Unknown function: " + funcName);
    }
}

std::shared_ptr<Expression> ExpressionParser::createEqualityExpression(const std::string& funcName, const std::string& arg, const std::string& value, bool isEqual) {
    if (funcName == "getType") {
        if (isEqual) {
            return std::make_shared<ValueExpression>([value](std::shared_ptr<NodeNN> node) -> bool {
                return node->getType() == value;
            });
        } else {
            return std::make_shared<ValueExpression>([value](std::shared_ptr<NodeNN> node) -> bool {
                return node->getType() != value;
            });
        }
    } else {
        throw std::runtime_error("Unknown function or unsupported operation: " + funcName);
    }
}

std::shared_ptr<Expression> ExpressionParser::parse() {
    auto expr = parseExpression();
    skipWhitespace();
    if (mPos != input.length()) {
        throw std::runtime_error("Unexpected characters at end of expression");
    }
    return expr;
}

}