#pragma once
#include <regex>
#include <stdexcept>
#include <string>

namespace CorGIReg {
// Strictness is per occurrence; identity depends only on predicate and suffix.
struct CommonKey {
    std::string predicate, suffix;
    bool strictInputs = false, strictOutputs = false;
    std::string identity() const { return predicate + "#" + suffix; }
};

inline CommonKey parseCommonKey(const std::string& text) {
    static const std::regex syntax(R"((\$?)([A-Za-z_0-9]+)([#$]?)([0-9]*))");
    std::smatch parts;
    if (!std::regex_match(text, parts, syntax) ||
        (parts[1].str().empty() && parts[3].str().empty()))
        throw std::invalid_argument("Invalid common node: " + text);
    auto suffix = parts[4].str();
    if (!suffix.empty()) {
        const auto first = suffix.find_first_not_of('0');
        suffix = first == std::string::npos ? "0" : suffix.substr(first);
    }
    return {parts[2].str(), suffix, !parts[1].str().empty(), parts[3].str() == "$"};
}
}
