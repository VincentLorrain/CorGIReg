#include "NodeTest/NodeValueJson.hpp"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace CorGIReg {
namespace {
class Reader {
public:
    explicit Reader(const std::string& text) : text_(text) {
        if (text_.compare(0, 3, "\xEF\xBB\xBF") == 0) pos_ = 3;
    }
    NodeValue document() {
        auto value = parse(0);
        space();
        if (pos_ != text_.size()) fail("unexpected data after the JSON value");
        return value;
    }
private:
    [[noreturn]] void fail(const std::string& message) const {
        std::size_t line = 1, column = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') { ++line; column = 1; }
            else if ((static_cast<unsigned char>(text_[i]) & 0xC0) != 0x80) ++column;
        }
        throw std::invalid_argument("JSON line " + std::to_string(line) + ", column " +
                                    std::to_string(column) + ": " + message);
    }
    void space() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                                       text_[pos_] == '\n' || text_[pos_] == '\r')) ++pos_;
    }
    bool consume(char c) {
        space();
        if (pos_ < text_.size() && text_[pos_] == c) { ++pos_; return true; }
        return false;
    }
    void literal(const char* word) {
        for (const char* c = word; *c; ++c, ++pos_)
            if (pos_ >= text_.size() || text_[pos_] != *c) fail("invalid literal");
    }
    NodeValue parse(unsigned depth) {
        if (depth > 64) fail("nesting exceeds 64");
        space();
        if (pos_ >= text_.size()) fail("unexpected end of input");
        switch (text_[pos_]) {
        case '{': {
            ++pos_;
            NodeValue::Object result;
            if (consume('}')) return result;
            do {
                space();
                if (pos_ >= text_.size() || text_[pos_] != '"') fail("expected an object key");
                auto key = string();
                if (!consume(':')) fail("expected ':'");
                if (!result.emplace(key, parse(depth + 1)).second) fail("duplicate object key \"" + key + "\"");
            } while (consume(','));
            if (!consume('}')) fail("expected ',' or '}'");
            return result;
        }
        case '[': {
            ++pos_;
            NodeValue::Array result;
            if (consume(']')) return result;
            do result.push_back(parse(depth + 1)); while (consume(','));
            if (!consume(']')) fail("expected ',' or ']'");
            return result;
        }
        case '"': return string();
        case 't': literal("true"); return true;
        case 'f': literal("false"); return false;
        case 'n': literal("null"); return nullptr;
        default: return number();
        }
    }
    unsigned hex4() {
        if (pos_ + 4 > text_.size()) fail("incomplete unicode escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= c - '0';
            else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
            else fail("invalid unicode escape");
        }
        return value;
    }
    static void utf8(std::string& out, unsigned code) {
        if (code < 0x80) out += static_cast<char>(code);
        else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    // Copies one validated UTF-8 sequence starting at pos_.
    void sequence(std::string& out) {
        const auto lead = static_cast<unsigned char>(text_[pos_]);
        const std::size_t size = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 0;
        if (!size || pos_ + size > text_.size()) fail("invalid UTF-8");
        unsigned code = size == 1 ? lead : lead & (0x7F >> size);
        for (std::size_t i = 1; i < size; ++i) {
            const auto next = static_cast<unsigned char>(text_[pos_ + i]);
            if ((next & 0xC0) != 0x80) fail("invalid UTF-8");
            code = (code << 6) | (next & 0x3F);
        }
        static const unsigned minimum[] = {0, 0, 0x80, 0x800, 0x10000};
        if (code < minimum[size] || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) fail("invalid UTF-8");
        out.append(text_, pos_, size);
        pos_ += size;
    }
    std::string string() {
        ++pos_;
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) fail("unterminated string");
            const char c = text_[pos_];
            if (c == '"') { ++pos_; return out; }
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { sequence(out); continue; }
            if (++pos_ >= text_.size()) fail("unterminated string");
            const char escape = text_[pos_++];
            switch (escape) {
            case '"': case '\\': case '/': out += escape; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                auto code = hex4();
                if (code >= 0xDC00 && code <= 0xDFFF) fail("unpaired surrogate");
                if (code >= 0xD800 && code <= 0xDBFF) {
                    if (pos_ + 2 > text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u') fail("unpaired surrogate");
                    pos_ += 2;
                    const auto low = hex4();
                    if (low < 0xDC00 || low > 0xDFFF) fail("unpaired surrogate");
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                utf8(out, code);
                break;
            }
            default: fail("invalid escape");
            }
        }
    }
    NodeValue number() {
        const auto start = pos_;
        auto digits = [&] {
            const auto first = pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
            return pos_ - first;
        };
        const bool negative = pos_ < text_.size() && text_[pos_] == '-';
        if (negative) ++pos_;
        const auto integerStart = pos_;
        const auto count = digits();
        if (!count) fail("invalid value");
        if (count > 1 && text_[integerStart] == '0') fail("leading zero in number");
        bool integer = true;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_; integer = false;
            if (!digits()) fail("invalid number fraction");
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_; integer = false;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (!digits()) fail("invalid number exponent");
        }
        const auto token = text_.substr(start, pos_ - start);
        if (integer) {
            errno = 0;
            char* end = nullptr;
            if (negative) {
                const auto value = std::strtoll(token.c_str(), &end, 10);
                if (errno == ERANGE) fail("integer outside the supported 64-bit range");
                return NodeValue(std::int64_t(value));
            }
            const auto value = std::strtoull(token.c_str(), &end, 10);
            if (errno == ERANGE) fail("integer outside the supported 64-bit range");
            return NodeValue(std::uint64_t(value));
        }
        std::istringstream stream(token);
        stream.imbue(std::locale::classic());
        double value;
        if (!(stream >> value) || !std::isfinite(value)) fail("number is not a finite double");
        return value;
    }
    const std::string& text_;
    std::size_t pos_ = 0;
};

void writeString(std::ostream& out, const std::string& text) {
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << c;
    }
    out << '"';
}

struct Writer {
    std::ostream& out;
    void operator()(std::nullptr_t) const { out << "null"; }
    void operator()(bool value) const { out << (value ? "true" : "false"); }
    void operator()(std::int64_t value) const { out << value; }
    void operator()(std::uint64_t value) const { out << value; }
    void operator()(double value) const {
        if (!std::isfinite(value)) throw std::invalid_argument("Cannot write a non-finite number as JSON");
        std::string text;
        for (int precision = 1; precision <= 17; ++precision) {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream.precision(precision);
            stream << value;
            text = stream.str();
            if (std::stod(text) == value) break;
        }
        if (text.find_first_of(".eEn") == std::string::npos) text += ".0";
        out << text;
    }
    void operator()(const std::string& value) const { writeString(out, value); }
    void operator()(const NodeValue::Array& value) const {
        out << '[';
        for (std::size_t i = 0; i < value.size(); ++i) { if (i) out << ','; std::visit(*this, value[i].data); }
        out << ']';
    }
    void operator()(const NodeValue::Object& value) const {
        out << '{';
        bool first = true;
        for (const auto& entry : value) {
            if (!first) out << ',';
            first = false;
            writeString(out, entry.first); out << ':';
            std::visit(*this, entry.second.data);
        }
        out << '}';
    }
    void operator()(NodeValue::Unsupported) const { throw std::invalid_argument("Cannot write an unsupported value as JSON"); }
    void operator()(NodeValue::Missing) const { throw std::invalid_argument("Cannot write a missing value as JSON"); }
};
}

NodeValue parseJson(const std::string& text) { return Reader(text).document(); }

void writeJson(std::ostream& output, const NodeValue& value) { std::visit(Writer{output}, value.data); }

}
