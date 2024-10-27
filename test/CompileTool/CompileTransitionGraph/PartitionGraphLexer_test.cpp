#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"

#include <iostream>
#include <map>
#include <functional>
#include <random>
#include <string>

using namespace CorGIReg;

TEST_CASE("PartitionGraphLexer") {
    SECTION("RandomGenerateTest") {
        // Use modern C++ random number generation
        std::random_device rd;
        std::mt19937 gen(rd());

        // Define character sets
        const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890_";
        const std::string digits = "1234567890";

        // Create distributions
        std::uniform_int_distribution<> len_dist(1, 20);
        std::uniform_int_distribution<> char_dist(0, characters.size() - 1);
        std::uniform_int_distribution<> digit_dist(0, digits.size() - 1);

        // Map of token generators
        std::map<tGTokensTypes, std::function<std::pair<std::string, std::string>()>> LexerTestMap{
            {tGTokensTypes::NEXT,     []() -> std::pair<std::string, std::string> { return {"-> ", ""}; }},
            {tGTokensTypes::PREC,     []() -> std::pair<std::string, std::string> { return {"<- ", ""}; }},
            {tGTokensTypes::QOM,      []() -> std::pair<std::string, std::string> { return {"+ ", ""}; }},
            {tGTokensTypes::QZM,      []() -> std::pair<std::string, std::string> { return {"* ", ""}; }},
            {tGTokensTypes::SEP,      []() -> std::pair<std::string, std::string> { return {"; ", ""}; }},
            {tGTokensTypes::END,      []() -> std::pair<std::string, std::string> { return {"$ ", ""}; }},
            {tGTokensTypes::QZO,      []() -> std::pair<std::string, std::string> { return {"? ", ""}; }},
            {tGTokensTypes::LPAREN,   []() -> std::pair<std::string, std::string> { return {"( ", ""}; }},
            {tGTokensTypes::RPAREN,   []() -> std::pair<std::string, std::string> { return {") ", ""}; }},

            {tGTokensTypes::KEY,   [&]() -> std::pair<std::string, std::string> {
                std::size_t keyLen = len_dist(gen);
                std::string key;
                for (std::size_t i = 0; i < keyLen; ++i) {
                    key += characters[char_dist(gen)];
                }
                return {key + " ", key};
            }},

            {tGTokensTypes::CKEY,   [&]() -> std::pair<std::string, std::string> {
                std::size_t keyLen = len_dist(gen);
                std::string key, idx;
                for (std::size_t i = 0; i < keyLen; ++i) {
                    key += characters[char_dist(gen)];
                    idx += digits[digit_dist(gen)];
                }
                return {key + "#" + idx + " ", key + "#" + idx};
            }},

            {tGTokensTypes::SCKEY,   [&]() -> std::pair<std::string, std::string> {
                std::size_t keyLen = len_dist(gen);
                std::string key, idx;
                for (std::size_t i = 0; i < keyLen; ++i) {
                    key += characters[char_dist(gen)];
                    idx += digits[digit_dist(gen)];
                }
                return {key + "$" + idx + " ", key + "$" + idx};
            }}
        };

        // Generate test tokens
        const std::size_t numRandomElements = 1000;
        std::vector<std::tuple<tGTokensTypes, std::string>> testVector;
        std::string testString;

        // Create a vector of token types for indexing
        std::vector<tGTokensTypes> tokenTypes;
        for (const auto& pair : LexerTestMap) {
            tokenTypes.push_back(pair.first);
        }

        std::uniform_int_distribution<> token_dist(0, tokenTypes.size() - 1);

        for (std::size_t i = 0; i < numRandomElements; ++i) {
            tGTokensTypes randomKey = tokenTypes[token_dist(gen)];
            auto result = LexerTestMap[randomKey]();

            testString += result.first;
            testVector.emplace_back(randomKey, result.second);
        }

        // Initialize the lexer with the test string
        PartitionGraphLexer graphLexer(testString);

        // Test the tokens
        for (const auto& testToken : testVector) {
            tGTokensTypes expectedType = std::get<0>(testToken);
            const std::string& expectedLexeme = std::get<1>(testToken);

            std::shared_ptr<ParsingToken<tGTokensTypes>> token = graphLexer.getNextToken();

            // Provide detailed information on failure
            INFO("Expected Token Type: " << static_cast<int>(expectedType)
                 << ", Expected Lexeme: '" << expectedLexeme << "'"
                 << ", Got Token Type: " << static_cast<int>(token->getType())
                 << ", Got Lexeme: '" << token->getLexeme() << "'");

            REQUIRE(token->getType() == expectedType);
            REQUIRE(token->getLexeme() == expectedLexeme);
        }

        // Ensure the lexer reaches the STOP token at the end
        std::shared_ptr<ParsingToken<tGTokensTypes>> token = graphLexer.getNextToken();
        REQUIRE(token->getType() == tGTokensTypes::STOP);
    }
}
