#include "CompileTool/Common/NodePattern.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include <cctype>
#include <algorithm>
#include <set>

namespace CorGIReg {
namespace {
bool keyChar(char c) {
    return (std::isalnum(static_cast<unsigned char>(c)) && static_cast<unsigned char>(c) < 128) || c == '_';
}
std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
}

NodePattern parseNodePattern(const std::string& input) {
    const auto text = trim(input);
    auto invalid = [&]() { throw std::invalid_argument("Invalid node condition: " + input); };
    NodePattern result;
    std::size_t pos = 0;
    const bool prefix = pos < text.size() && text[pos] == '$';
    if (prefix) ++pos;
    const auto begin = pos;
    while (pos < text.size() && keyChar(text[pos])) ++pos;
    result.name = text.substr(begin, pos - begin);
    if (result.name.empty()) invalid();
    const auto nameEnd = pos;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos < text.size() && text[pos] == '(') {
        const auto conditionBegin = ++pos;
        std::size_t itemBegin = pos;
        int depth = 0;
        bool quoted = false, escaped = false, closed = false;
        std::set<std::string> keys;
        for (; pos < text.size(); ++pos) {
            const auto c = text[pos];
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
                continue;
            }
            if (c == '"') { quoted = true; continue; }
            if (c == '[' || c == '{') ++depth;
            if (c == ']' || c == '}') { if (--depth < 0) invalid(); }
            if ((c == ',' || c == ')') && depth == 0) {
                const auto item = trim(text.substr(itemBegin, pos - itemBegin));
                const auto eq = item.find('=');
                if (eq == std::string::npos) invalid();
                const auto key = trim(item.substr(0, eq));
                if (key.empty() || !std::all_of(key.begin(), key.end(), keyChar) || !keys.insert(key).second) invalid();
                const auto rhs = trim(item.substr(eq + 1));
                if (rhs.empty()) invalid();
                result.conditions.emplace_back(key, parseJson(rhs));
                itemBegin = pos + 1;
                if (c == ')') { closed = true; ++pos; break; }
            }
        }
        if (!closed || quoted || depth != 0 || pos == conditionBegin) invalid();
    }
    const auto predicateEnd = pos;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    const auto suffixBegin = pos;
    if (pos < text.size() && (text[pos] == '#' || text[pos] == '$')) {
        ++pos;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
    }
    if (pos != text.size()) invalid();
    result.predicate = result.conditions.empty() ? result.name : result.name + text.substr(nameEnd, predicateEnd - nameEnd);
    result.anchor = (prefix ? "$" : "") + result.name + text.substr(suffixBegin);
    return result;
}
}
