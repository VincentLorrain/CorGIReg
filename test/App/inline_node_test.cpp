#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "NodeTest/NodeLibrary.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include "NodeTest/NodeDictionary.hpp"

using namespace CorGIReg;

TEST_CASE("Inline node matching resolves all IR key aliases and keeps source IDs", "[inline-node][app]") {
    NodeLibrary library;
    library.add(NodeDefinition::fromDocument(parseJson(R"({"schema_version":4,"name":"conv",
        "key_matching":{"kernel_shape":{"onnx":"onnx_kernel","stablehlo":"filter_size"}},
        "formats":{"json":{"op_types":["Conv"]},"onnx":{"op_types":["Conv"]},
        "stablehlo":{"op_types":["stablehlo.convolution"]},"custom":{"op_types":["Conv"]}}})"), "conv.json"));
    for (const std::string format : {"json", "onnx", "stablehlo", "custom"}) {
        const std::string field = format == "onnx" ? "onnx_kernel" : format == "stablehlo" ? "filter_size" : "kernel_shape";
        const std::string operation = format == "stablehlo" ? "stablehlo.convolution" : "Conv";
        auto graph = std::make_shared<ImportedGraph>();
        for (auto id : {42, 7, 8, 9}) graph->addNode(id, "sourceConv", "conv");
        for (auto id : {42, 7, 8, 9}) {
            graph->addNode(id + 100, "Relu", "activation");
            graph->addEdge(id, id + 100);
        }
        const std::map<NodeId, NodeValue> documents{
            {42, NodeValue::Object{{"op", operation}, {"attributes", NodeValue::Object{{field, NodeValue::Array{3,3}}, {"strides", NodeValue::Array{1,1}}}}}},
            {7, NodeValue::Object{{"op", operation}, {"attributes", NodeValue::Object{{field, NodeValue::Array{5,5}}}}}},
            {8, NodeValue::Object{{"op", operation}, {"attributes", NodeValue::Object{{field, "3x3"}}}}},
            {9, NodeValue::Object{{"op", operation}, {"attributes", NodeValue::Object{}}}},
            {142, NodeValue::Object{{"op", "Relu"}}}, {107, NodeValue::Object{{"op", "Relu"}}},
            {108, NodeValue::Object{{"op", "Relu"}}}, {109, NodeValue::Object{{"op", "Relu"}}}};
        for (const std::string alias : {"kernel_shape", "onnx_kernel", "filter_size"}) {
            const auto filtered = "conv(" + alias + "=[3,3])";
            for (const auto& query : {filtered + "->Relu", "Relu<-" + filtered,
                filtered + "?->Relu", filtered + "#->Relu;conv(kernel_shape=[3,3])#", "$" + filtered + "$->Relu"}) {
                INFO(format << ": " << query);
                auto pattern = PGInterpreterTG(query).interpretRooted();
                NodePredicates predicates{{"Relu", [](NodeId id) { return id >= 100; }}};
                addDefinitionPredicates(predicates, library.resolve(format, documents));
                std::set<std::string> keys;
                for (const auto& transition : pattern->getTransitions()) keys.insert(transition->getTestKey());
                addInlineNodePredicates(predicates, keys, documents, &library, format);
                std::vector<std::optional<NodeId>> starts(pattern->getNbStart(), 42);
                const auto result = pattern->test(graph, starts, predicates);
                REQUIRE(result->isMatch());
                REQUIRE(result->getMatches().at(0)->getAll().count(42) == 1);
                REQUIRE(result->getMatches().at(0)->getAll().count(142) == 1);
                for (const auto id : {7, 8, 9}) {
                    std::fill(starts.begin(), starts.end(), id);
                    REQUIRE_FALSE(pattern->test(graph, starts, predicates)->isMatch());
                }
            }
        }
        NodePredicates predicates;
        addDefinitionPredicates(predicates, library.resolve(format, documents));
        addInlineNodePredicates(predicates, {"conv(kernel_shape=[3,3],strides=[1,1])",
            "conv(kernel_shape=[3,3],strides=[2,2])"}, documents, &library, format);
        REQUIRE(predicates.at("conv(kernel_shape=[3,3],strides=[1,1])")(42));
        REQUIRE_FALSE(predicates.at("conv(kernel_shape=[3,3],strides=[2,2])")(42));
        auto contradictory = PGInterpreterTG(
            "conv(kernel_shape=[3,3])#->Relu;conv(kernel_shape=[5,5])#").interpretRooted();
        predicates["Relu"] = [](NodeId id) { return id >= 100; };
        addInlineNodePredicates(predicates, {"conv(kernel_shape=[5,5])", "conv(kernel_shape=[3,3])"},
                                documents, &library, format);
        REQUIRE_FALSE(contradictory->test(graph,
            std::vector<std::optional<NodeId>>(contradictory->getNbStart(), 42), predicates)->isMatch());
    }
}

TEST_CASE("Legacy NodeNN matching evaluates inline attributes without global filtered keys", "[inline-node][app]") {
    REQUIRE(NodeDictionary::registerFunction("InlineLegacyConv", [](NodeNN::Ptr node) { return node->getType() == "InlineLegacyConv"; }));
    REQUIRE(NodeDictionary::registerFunction("InlineLegacyRelu", [](NodeNN::Ptr node) { return node->getType() == "InlineLegacyRelu"; }));
    GraphNN graph;
    auto conv = std::make_shared<NodeNN>(), relu = std::make_shared<NodeNN>();
    conv->setInfo("Type", std::string("InlineLegacyConv"));
    conv->setInfo("kernel_shape", std::vector<int>{3,3});
    relu->setInfo("Type", std::string("InlineLegacyRelu"));
    graph.addEdge(conv, relu);
    auto pattern = PGInterpreterTG("InlineLegacyConv(kernel_shape=[3,3])->InlineLegacyRelu").interpret();
    REQUIRE(pattern->test({conv})->isMatch());
    conv->setInfo("kernel_shape", std::vector<int>{5,5});
    REQUIRE_FALSE(pattern->test({conv})->isMatch());
    REQUIRE_FALSE(NodeDictionary::hasFunction("InlineLegacyConv(kernel_shape=[3,3])"));
}
