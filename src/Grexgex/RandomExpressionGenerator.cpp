#include "Grexgex/RandomExpressionGenerator.h"
#include <ctime>
#include <cstdlib>   // std::rand, std::srand

namespace CorGIReg {
// --- RNG holder ---
std::mt19937& RandomExpressionGenerator::getGen() {
    static std::mt19937 gen(static_cast<unsigned int>(std::time(nullptr)));
    return gen;
}

// --- Helpers ---
std::vector<std::string>& RandomExpressionGenerator::dict() {
    static std::vector<std::string> d;
    return d;
}

void RandomExpressionGenerator::setDictionary(const std::vector<std::string>& words) {
    dict() = words;
}

void RandomExpressionGenerator::clearDictionary() {
    dict().clear();
}

std::string RandomExpressionGenerator::generateRandomString() {
    auto& gen = getGen();

    // If dictionary is defined → always use it
    if (!dict().empty()) {
        std::uniform_int_distribution<std::size_t> pick(0, dict().size() - 1);
        return dict()[pick(gen)];
    }

    // Otherwise → generate random A–Z string (length ≥ 1)
    std::uniform_int_distribution<int> disChar('A', 'Z');
    std::uniform_int_distribution<int> disLen(1, 5);
    int len = disLen(gen);

    std::string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i)
        s += static_cast<char>(disChar(gen));
    return s;
}

// --exp --


std::string RandomExpressionGenerator::exp() {
    int randomValue = std::rand() % 7;
    switch (randomValue) {
        case 0: return generateRandomString();
        case 1: return generateRandomString() + "*";
        case 2: return generateRandomString() + "+";
        case 3: return generateRandomString() + "?";
        case 4: return generateRandomString() + "#";
        case 5: return generateRandomString() + "$";
        default: return domain();
    }
}

std::string RandomExpressionGenerator::rseqEnd() {
    return "$<-" + rseq();
}

std::string RandomExpressionGenerator::rseq() {
    int randomValue = std::rand() % 2;
    switch (randomValue) {
        case 0: return exp();
        default: return exp() + "<-" + rseq();
    }
}

std::string RandomExpressionGenerator::seq() {
    int randomValue = std::rand() % 3;
    switch (randomValue) {
        case 0: return exp();
        case 1: return exp() + "->" + seq();
        default: return exp() + "->$";
    }
}

std::string RandomExpressionGenerator::domain() {
    int randomValue = std::rand() % 9;
    switch (randomValue) {
        case 0: return "(" + seq() + ")*";
        case 1: return "(" + seq() + ")?";
        case 3: return "(" + seq() + ")+";

        case 4: return "(" + rseq() + ")*";
        case 5: return "(" + rseq() + ")?";
        case 6: return "(" + rseq() + ")+";

        case 7: return "(" + rseqEnd() + ")*";
        case 8: return "(" + rseqEnd() + ")?";
        default: return "(" + rseqEnd() + ")+";

    }
}

std::string RandomExpressionGenerator::allExpr() {
    int randomValue = std::rand() % 6;
    switch (randomValue) {
        case 0:  return seq();
        case 1:  return seq() + ";" + allExpr();

        case 2:  return rseq();
        case 3:  return rseq() + ";" + allExpr();

        case 4:  return rseqEnd();
        default:  return rseqEnd() + ";" + allExpr();
    }

}

}