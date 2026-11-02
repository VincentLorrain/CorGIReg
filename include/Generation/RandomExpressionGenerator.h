#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace CorGIReg {
// Test/example generator. Configuration and generation are sequential operations.
class RandomExpressionGenerator {
    RandomExpressionGenerator() = default;
    static std::mt19937& getGen();
    static std::vector<std::string>& dict();
    static unsigned pick(unsigned count);
    static std::string generateRandomString();
    static std::string exp(unsigned depth);
    static std::string sequence(unsigned depth, bool reverse);
public:
    // At most three paths, three units per sequence and maxDepth nested groups.
    // Accepted maximum depth: 0..6. Default preserves the no-argument API.
    static std::string allExpr(unsigned maxDepth = 3);
    static void setSeed(std::uint32_t seed);
    static void setDictionary(const std::vector<std::string>& words);
    static void clearDictionary();
};
}
