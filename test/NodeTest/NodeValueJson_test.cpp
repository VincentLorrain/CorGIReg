#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeValueJson.hpp"
#include <sstream>

using namespace CorGIReg;
namespace {
using V = NodeValue;
using O = V::Object;
using A = V::Array;
std::string json(const V& value) { std::ostringstream out; writeJson(out, value); return out.str(); }
}

TEST_CASE("JSON reader keeps integer signedness doubles and nesting", "[NodeTest][json]") {
    const auto value = parseJson("\xEF\xBB\xBF { \"a\": [1, -2, 2.5, 1e2, true, false, null],\n"
                                 "\"s\": \"\\u00e9\\ud83d\\ude00\\n\\\"\", \"big\": 18446744073709551615 }");
    const auto& object = std::get<O>(value.data);
    const auto& array = std::get<A>(object.at("a").data);
    REQUIRE(std::get<std::uint64_t>(array[0].data) == 1);
    REQUIRE(std::get<std::int64_t>(array[1].data) == -2);
    REQUIRE(std::get<double>(array[2].data) == 2.5);
    REQUIRE(std::get<double>(array[3].data) == 100.0);
    REQUIRE(std::get<bool>(array[4].data));
    REQUIRE_FALSE(std::get<bool>(array[5].data));
    REQUIRE(std::holds_alternative<std::nullptr_t>(array[6].data));
    REQUIRE(std::get<std::string>(object.at("s").data) == "\xC3\xA9\xF0\x9F\x98\x80\n\"");
    REQUIRE(std::get<std::uint64_t>(object.at("big").data) == 18446744073709551615ull);
}

TEST_CASE("JSON reader rejects malformed or unsupported documents with a position", "[NodeTest][json]") {
    for (const std::string text : {"", "{", "[1,]", "{\"a\":1,}", "01", "1.", "-", "1e", "NaN", "Infinity",
                                   "tru", "\"a", "\"\\x\"", "\"\\ud800\"", "\"\\udc00x\"", "{\"a\":1,\"a\":2}",
                                   "18446744073709551616", "-9223372036854775809", "1e400", "\"\x01\"",
                                   "\"\xC3\"", "\"\xC0\xAF\"", "\"\xED\xA0\x80\"", "[] []", "{1:2}"}) {
        INFO(text);
        REQUIRE_THROWS_AS(parseJson(text), std::invalid_argument);
    }
    try { parseJson("{\n  \"a\": x}"); FAIL("expected a JSON error"); }
    catch (const std::invalid_argument& error) { REQUIRE(std::string(error.what()) == "JSON line 2, column 8: invalid value"); }
    std::string deep(65, '['), closed(65, ']');
    REQUIRE_NOTHROW(parseJson(deep.substr(1) + closed.substr(1)));
    REQUIRE_THROWS_AS(parseJson(deep + "0" + closed), std::invalid_argument);
}

TEST_CASE("JSON writer round-trips values with stable doubles", "[NodeTest][json]") {
    const V value = O{{"b", A{V(std::uint64_t(3)), V(std::int64_t(-4)), 0.1, 3.0, 1e300, nullptr, true}},
                      {"a", "q\"\\\n\x01\xC3\xA9"}};
    const auto text = json(value);
    REQUIRE(text == "{\"a\":\"q\\\"\\\\\\u000a\\u0001\xC3\xA9\",\"b\":[3,-4,0.1,3.0,1e+300,null,true]}");
    REQUIRE(json(parseJson(text)) == text);
    REQUIRE_THROWS_AS(json(V::Missing{}), std::invalid_argument);
    REQUIRE_THROWS_AS(json(A{V::Unsupported{}}), std::invalid_argument);
}
