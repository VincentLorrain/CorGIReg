
#pragma once


#include <string>
#include <random>

namespace CorGIReg {
class RandomExpressionGenerator {
private:
    // Prevent instantiation
    RandomExpressionGenerator() = default;

    // Single RNG instance (initialized once, thread-safe since C++11)
    static std::mt19937& getGen();

    // --- dictionary controls ---
    static std::vector<std::string>& dict();      // storage for allowed strings

    // Helpers (all static)
    static std::string generateRandomString();
    static std::string domain();
    static std::string exp();
    static std::string seq();
    static std::string rseq();
    static std::string rseqEnd();

public:
    // Public API
    static std::string allExpr();

    // Configure dictionary (copy is fine for a small list; you can move if needed)
    static void setDictionary(const std::vector<std::string>& words);
    static void clearDictionary(); // disables dictionary mode
};
}