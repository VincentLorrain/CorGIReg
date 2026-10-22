#include "NodeTest/NodeRule.hpp"
#include <cmath>
#include <optional>
#include <regex>
#include <set>

namespace CorGIReg {
namespace {
using V = NodeValue;
using O = V::Object;
using A = V::Array;
template<class T> const T* as(const V& v) { return std::get_if<T>(&v.data); }
[[noreturn]] void invalid(const std::string& path, const std::string& message) {
    throw std::invalid_argument(path + ": " + message);
}
std::optional<long double> numeric(const V& v) {
    if (auto p = as<std::int64_t>(v)) return *p;
    if (auto p = as<std::uint64_t>(v)) return *p;
    if (auto p = as<double>(v); p && std::isfinite(*p)) return *p;
    return {};
}
bool equal(const V& a, const V& b) {
    if (as<V::Unsupported>(a) || as<V::Unsupported>(b)) return false;
    if (auto x = numeric(a)) { auto y = numeric(b); return y && *x == *y; }
    if (a.data.index() != b.data.index()) return false;
    if (as<std::nullptr_t>(a)) return true;
    if (auto x = as<bool>(a)) return *x == *as<bool>(b);
    if (auto x = as<std::string>(a)) return *x == *as<std::string>(b);
    if (auto x = as<A>(a)) {
        const auto& y = *as<A>(b);
        return x->size() == y.size() && std::equal(x->begin(), x->end(), y.begin(), equal);
    }
    if (auto x = as<O>(a)) {
        const auto& y = *as<O>(b);
        if (x->size() != y.size()) return false;
        for (const auto& entry : *x) {
            auto it = y.find(entry.first);
            if (it == y.end() || !equal(entry.second, it->second)) return false;
        }
        return true;
    }
    return false;
}
bool compatible(const V& a, const V& b) {
    if ((as<double>(a) && !numeric(a)) || (as<double>(b) && !numeric(b))) return false;
    return !as<V::Unsupported>(a) && !as<V::Unsupported>(b) && !as<V::Missing>(a) && !as<V::Missing>(b) &&
        ((numeric(a) && numeric(b)) || a.data.index() == b.data.index());
}
A pathOf(const V& value, const std::string& path) {
    const auto parts = as<A>(value);
    if (!parts) invalid(path, "expected an array of string keys or nonnegative indices");
    for (const auto& part : *parts) {
        if (as<std::string>(part) || as<std::uint64_t>(part)) continue;
        if (auto i = as<std::int64_t>(part); i && *i >= 0) continue;
        invalid(path, "path segments must be strings or nonnegative integers");
    }
    return *parts;
}
const V* lookup(const V& root, const A& path) {
    const V* current = &root;
    for (const auto& part : path) {
        if (auto key = as<std::string>(part)) {
            auto object = as<O>(*current);
            if (!object) return nullptr;
            auto it = object->find(*key);
            if (it == object->end()) return nullptr;
            current = &it->second;
        } else {
            auto array = as<A>(*current);
            auto index = as<std::uint64_t>(part) ? *as<std::uint64_t>(part) : std::uint64_t(*as<std::int64_t>(part));
            if (!array || index >= array->size()) return nullptr;
            current = &(*array)[index];
        }
    }
    return as<V::Missing>(*current) ? nullptr : current;
}
std::regex regexOf(const V& value, const std::string& path) {
    auto pattern = as<std::string>(value);
    if (!pattern) invalid(path, "regex must be a string");
    try { return std::regex(*pattern, std::regex::ECMAScript); }
    catch (const std::regex_error& e) { invalid(path, std::string("invalid regex: ") + e.what()); }
}
NodeRule comparison(const std::string& op, const V& value, const std::string& path, unsigned depth);
NodeRule compile(const V& value, const std::string& path, unsigned depth) {
    if (depth > 64) invalid(path, "rule nesting exceeds 64");
    auto object = as<O>(value);
    if (!object || object->empty()) invalid(path, "expected a nonempty rule object");
    for (const auto& logical : {"all", "any", "not"}) {
        auto it = object->find(logical);
        if (it == object->end()) continue;
        if (object->size() != 1) invalid(path, "logical rules cannot contain other fields");
        if (std::string(logical) == "not") {
            auto child = compile(it->second, path + ".not", depth + 1);
            return [child](const V& doc) { return !child(doc); };
        }
        auto children = as<A>(it->second);
        if (!children || children->empty()) invalid(path, "all/any requires a nonempty rule array");
        std::vector<NodeRule> rules;
        for (std::size_t i = 0; i < children->size(); ++i)
            rules.push_back(compile((*children)[i], path + "." + logical + "[" + std::to_string(i) + "]", depth + 1));
        return [rules, all = std::string(logical) == "all"](const V& doc) {
            for (const auto& rule : rules) if (rule(doc) != all) return !all;
            return all;
        };
    }
    if (auto it = object->find("keys"); it != object->end()) {
        if (object->size() != 3 || !object->count("regex") || !object->count("count"))
            invalid(path, "keys requires exactly regex and count");
        auto parts = pathOf(it->second, path + ".keys");
        auto regex = regexOf(object->at("regex"), path + ".regex");
        auto count = as<O>(object->at("count"));
        if (!count || count->size() != 1) invalid(path, "count requires one numeric comparison");
        auto op = count->begin()->first;
        if (op != "eq" && op != "ne" && op != "gt" && op != "gte" && op != "lt" && op != "lte")
            invalid(path, "invalid count comparison");
        if (!numeric(count->begin()->second)) invalid(path, "count comparison requires a number");
        auto check = comparison(op, count->begin()->second, path + ".count", depth + 1);
        return [parts, regex, check](const V& doc) {
            auto found = lookup(doc, parts);
            auto keys = found ? as<O>(*found) : nullptr;
            if (!keys) return false;
            std::uint64_t n = 0;
            for (const auto& entry : *keys) if (std::regex_match(entry.first, regex)) ++n;
            return check(V(n));
        };
    }
    auto it = object->find("path");
    if (it == object->end() || object->size() != 2) invalid(path, "expected path and exactly one operator");
    auto parts = pathOf(it->second, path + ".path");
    auto operation = object->begin();
    if (operation->first == "path") ++operation;
    if (operation->first == "exists") {
        auto expected = as<bool>(operation->second);
        if (!expected) invalid(path, "exists requires a boolean");
        return [parts, present = *expected](const V& doc) { return (lookup(doc, parts) != nullptr) == present; };
    }
    auto check = comparison(operation->first, operation->second, path + "." + operation->first, depth + 1);
    return [parts, check](const V& doc) { auto found = lookup(doc, parts); return found && check(*found); };
}
NodeRule comparison(const std::string& op, const V& value, const std::string& path, unsigned depth) {
    if (depth > 64) invalid(path, "rule nesting exceeds 64");
    if (op == "eq" || op == "ne")
        return [value, eq = op == "eq"](const V& actual) { return compatible(actual, value) && equal(actual, value) == eq; };
    if (op == "in") {
        if (!as<A>(value)) invalid(path, "in requires an array");
        return [values = *as<A>(value)](const V& actual) {
            return std::any_of(values.begin(), values.end(), [&](const V& candidate) { return equal(actual, candidate); });
        };
    }
    if (op == "contains") return [value](const V& actual) {
        auto array = as<A>(actual);
        return array && std::any_of(array->begin(), array->end(), [&](const V& item) { return equal(item, value); });
    };
    if (op == "regex") {
        auto regex = regexOf(value, path);
        return [regex](const V& actual) { auto text = as<std::string>(actual); return text && std::regex_match(*text, regex); };
    }
    if (op == "length") {
        auto rule = as<O>(value);
        if (!rule || rule->size() != 1) invalid(path, "length requires one numeric comparison");
        const auto& entry = *rule->begin();
        if (!numeric(entry.second) || (entry.first != "eq" && entry.first != "ne" && entry.first != "gt" &&
            entry.first != "gte" && entry.first != "lt" && entry.first != "lte")) invalid(path, "invalid length comparison");
        auto check = comparison(entry.first, entry.second, path, depth + 1);
        return [check](const V& actual) {
            if (auto array = as<A>(actual)) return check(V(std::uint64_t(array->size())));
            if (auto object = as<O>(actual)) return check(V(std::uint64_t(object->size())));
            if (auto text = as<std::string>(actual)) return check(V(std::uint64_t(text->size())));
            return false;
        };
    }
    if (op == "gt" || op == "gte" || op == "lt" || op == "lte") {
        auto expected = numeric(value);
        if (!expected) invalid(path, "numeric comparison requires a finite number");
        return [expected = *expected, op](const V& actual) {
            auto n = numeric(actual);
            if (!n) return false;
            if (op == "gt") return *n > expected;
            if (op == "gte") return *n >= expected;
            if (op == "lt") return *n < expected;
            return *n <= expected;
        };
    }
    invalid(path, "unknown operator " + op);
}
V fromAny(const std::any& value) {
    if (!value.has_value() || std::any_cast<std::nullptr_t>(&value)) return nullptr;
    if (auto p = std::any_cast<V>(&value)) return *p;
    if (auto p = std::any_cast<bool>(&value)) return *p;
    if (auto p = std::any_cast<std::string>(&value)) return *p;
    if (auto p = std::any_cast<const char*>(&value)) return *p ? V(*p) : V(nullptr);
#define NUMBER(T, target) if (auto p = std::any_cast<T>(&value)) return V(static_cast<target>(*p));
    NUMBER(int, std::int64_t) NUMBER(long, std::int64_t) NUMBER(long long, std::int64_t)
    NUMBER(unsigned, std::uint64_t) NUMBER(unsigned long, std::uint64_t) NUMBER(unsigned long long, std::uint64_t)
    NUMBER(float, double) NUMBER(double, double)
#undef NUMBER
    if (auto p = std::any_cast<O>(&value)) return *p;
    if (auto p = std::any_cast<A>(&value)) return *p;
#define VECTOR(T) if (auto p = std::any_cast<std::vector<T>>(&value)) { A result; for (const auto& item : *p) result.push_back(fromAny(std::any(item))); return result; }
    VECTOR(int) VECTOR(std::int64_t) VECTOR(double) VECTOR(float) VECTOR(std::string)
#undef VECTOR
    if (auto p = std::any_cast<NodeNN::Dictionary>(&value)) {
        O result;
        for (const auto& entry : *p) result.emplace(entry.first, fromAny(entry.second));
        return result;
    }
    return V::Unsupported{};
}
}
NodeRule compileNodeRule(const NodeValue& rule) { return compile(rule, "where", 0); }
NodeValue nodeDocument(const NodeNN& node) {
    O attributes;
    for (const auto& entry : node.getInfoDictionary()) attributes.emplace(entry.first, fromAny(entry.second));
    O result{{"attributes", attributes}, {"op", node.getType()}};
    for (const auto& key : {"name", "domain", "inputs"}) {
        auto it = attributes.find(key);
        if (it != attributes.end()) {
            auto value = it->second;
            if (std::string(key) == "inputs") {
                if (auto array = std::get_if<A>(&value.data))
                    for (auto& item : *array)
                        if (auto name = as<std::string>(item); name && name->empty()) item = V::Missing{};
            }
            result.emplace(key, std::move(value));
        }
    }
    return result;
}
}
