#include "Grexgex/RandomExpressionGenerator.h"
#include <stdexcept>

namespace CorGIReg {
std::mt19937& RandomExpressionGenerator::getGen() {
    static std::mt19937 gen(5489u);
    return gen;
}
std::vector<std::string>& RandomExpressionGenerator::dict() {
    static std::vector<std::string> words;
    return words;
}
void RandomExpressionGenerator::setSeed(std::uint32_t seed) { getGen().seed(seed); }
unsigned RandomExpressionGenerator::pick(unsigned count) {
    return std::uniform_int_distribution<unsigned>(0, count - 1)(getGen());
}
void RandomExpressionGenerator::setDictionary(const std::vector<std::string>& words) {
    for (const auto& word : words)
        if (word.empty() || word.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            throw std::invalid_argument("Generator dictionary requires nonempty predicate keys");
    dict() = words;
}
void RandomExpressionGenerator::clearDictionary() { dict().clear(); }
std::string RandomExpressionGenerator::generateRandomString() {
    if (!dict().empty()) return dict()[pick(static_cast<unsigned>(dict().size()))];
    std::string key;
    const auto length = 1 + pick(5);
    for (unsigned i = 0; i < length; ++i) key += static_cast<char>('A' + pick(26));
    return key;
}
std::string RandomExpressionGenerator::exp(unsigned depth) {
    const auto kind = pick(depth ? 9 : 8);
    if (kind == 8) {
        const auto body = sequence(depth - 1, pick(2) != 0);
        return "(" + body + ")" + "*+?"[pick(3)];
    }
    auto key = generateRandomString();
    if (kind == 0) return key;
    if (kind <= 3) return key + "*+?"[kind - 1];
    const auto suffix = pick(3) == 0 ? std::to_string(pick(3)) : std::string{};
    if (kind == 4) return key + "#" + suffix;
    if (kind == 5) return "$" + key + (suffix.empty() ? "" : "#" + suffix);
    if (kind == 6) return key + "$" + suffix;
    return "$" + key + "$" + suffix;
}
std::string RandomExpressionGenerator::sequence(unsigned depth, bool reverse) {
    const bool boundary = pick(4) == 0;
    std::string out = reverse && boundary ? "$<- " : "";
    const auto size = 1 + pick(3);
    for (unsigned i = 0; i < size; ++i) {
        if (i) out += reverse ? "<- " : "-> ";
        out += exp(depth);
    }
    if (!reverse && boundary) out += "->$";
    return out;
}
std::string RandomExpressionGenerator::allExpr(unsigned maxDepth) {
    if (maxDepth > 6) throw std::invalid_argument("Generator depth must be between 0 and 6");
    std::string out;
    const auto paths = 1 + pick(3);
    for (unsigned i = 0; i < paths; ++i) {
        if (i) out += ';';
        out += sequence(maxDepth, pick(2) != 0);
    }
    return out;
}
}
