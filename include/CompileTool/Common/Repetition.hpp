#pragma once

#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"
#include <charconv>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace CorGIReg {

struct Repetition {
    std::size_t minimum;
    std::optional<std::size_t> maximum;
    char symbol;
    std::string notation; // Complete spelling for a brace quantifier.

    bool allows(std::size_t count) const {
        return count >= minimum && (!maximum || count <= *maximum);
    }
    bool mayRepeat() const { return !maximum || *maximum > 1; }
};

inline bool isQuantifier(tGTokensTypes type) {
    using T = tGTokensTypes;
    return type == T::QOM || type == T::QZM || type == T::QZO || type == T::QRANGE;
}

inline Repetition parseRepetition(tGTokensTypes type, std::string_view notation = {}) {
    using T = tGTokensTypes;
    switch (type) {
    case T::QOM: return {1, std::nullopt, '+', {}};
    case T::QZM: return {0, std::nullopt, '*', {}};
    case T::QZO: return {0, 1, '?', {}};
    case T::QRANGE: break;
    default: throw std::logic_error("Expected a quantifier");
    }
    if (notation.size() < 3 || notation.front() != '{' || notation.back() != '}')
        throw std::invalid_argument("Expected {count}, {min,}, or {min,max}");
    auto trim = [](std::string_view value) {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.remove_prefix(1);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.remove_suffix(1);
        return value;
    };
    auto number = [&](std::string_view value) {
        value = trim(value);
        if (value.empty()) throw std::invalid_argument("Missing repetition count");
        std::size_t count = 0;
        auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || count > 1000000)
            throw std::invalid_argument("Repetition counts must be integers from 0 to 1000000");
        return count;
    };
    const auto body = notation.substr(1, notation.size() - 2);
    const auto comma = body.find(',');
    const auto minimum = number(body.substr(0, comma));
    std::optional<std::size_t> maximum = minimum;
    if (comma != std::string_view::npos) {
        const auto upper = trim(body.substr(comma + 1));
        if (upper.empty()) maximum.reset();
        else maximum = number(upper);
    }
    if (maximum && *maximum < minimum) throw std::invalid_argument("Maximum repetition is less than minimum");
    return {minimum, maximum, '{', std::string(notation)};
}

} // namespace CorGIReg
