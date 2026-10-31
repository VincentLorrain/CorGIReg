#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeLibrary.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include <sstream>

using namespace CorGIReg;

TEST_CASE("Key mappings use JSON defaults and accept aliases from every IR", "[inline-node][library]") {
    const auto definition = NodeDefinition::fromDocument(parseJson(R"({
        "schema_version":4,"name":"conv",
        "formats":{"json":{"op_types":["Conv"]}},
        "key_matching":{
            "kernel_shape":{"onnx":"onnx_kernel","stablehlo":"filter_size"},
            "strides":{},
            "group":{"json":"groups","onnx":["attributes","nested",0,"count"]}
        }})"), "key-test.json");
    auto pathMatches = [&](const std::string& key, const std::string& format, NodeValue::Array path) {
        std::ostringstream actual, wanted;
        writeJson(actual, definition.keyPath(key, format));
        writeJson(wanted, path);
        REQUIRE(actual.str() == wanted.str());
        const auto rule = compileNodeRule(NodeValue::Object{{"path", definition.keyPath(key, format)}, {"eq", 7}});
        const auto expected = compileNodeRule(NodeValue::Object{{"path", std::move(path)}, {"eq", 7}});
        const auto document = parseJson(R"({"attributes":{"kernel_shape":7,"strides":7,"groups":7,
            "onnx_kernel":7,"filter_size":7,"nested":[{"count":7}]}})");
        REQUIRE(rule(document));
        REQUIRE(expected(document));
        REQUIRE_FALSE(rule(parseJson(R"({"attributes":{}})")));
    };
    for (const auto* alias : {"kernel_shape", "onnx_kernel", "filter_size"}) {
        pathMatches(alias, "json", {"attributes", "kernel_shape"});
        pathMatches(alias, "onnx", {"attributes", "onnx_kernel"});
        pathMatches(alias, "stablehlo", {"attributes", "filter_size"});
        pathMatches(alias, "custom", {"attributes", "kernel_shape"});
        const auto path = definition.keyPath(alias, "json");
        REQUIRE(std::get<std::string>(path.back().data) == "kernel_shape");
    }
    for (const auto* format : {"json", "onnx", "stablehlo", "custom"}) {
        pathMatches("strides", format, {"attributes", "strides"});
        REQUIRE(std::get<std::string>(definition.keyPath("unmapped", format).back().data) == "unmapped");
    }
    pathMatches("count", "onnx", {"attributes", "nested", 0, "count"});
    pathMatches("count", "stablehlo", {"attributes", "groups"});
}

TEST_CASE("Invalid and ambiguous key mappings fail during definition loading", "[inline-node][library]") {
    for (const auto* mappings : {"[]", R"({"kernel_shape":[]})", R"({"kernel_shape":{"onnx":3}})",
        R"({"kernel_shape":{"onnx":[]}})", R"({"kernel_shape":{"onnx":[-1]}})",
        R"({"a":{"onnx":"same"},"b":{"stablehlo":"same"}})",
        R"({"a":{"onnx":"b"},"b":{}})", R"({"a":{"":"b"}})"}) {
        INFO(mappings);
        auto document = parseJson(R"({"schema_version":4,"name":"conv","formats":{"json":{"op_types":["Conv"]}}})");
        std::get<NodeValue::Object>(document.data).emplace("key_matching", parseJson(mappings));
        REQUIRE_THROWS_AS(NodeDefinition::fromDocument(document, "invalid-key.json"), std::invalid_argument);
    }
    REQUIRE_THROWS_AS(NodeDefinition::fromDocument(parseJson(R"({"schema_version":3,"name":"conv",
        "formats":{"json":{"op_types":["Conv"]}},"key_matching":{}})"), "old-schema.json"), std::invalid_argument);
}
