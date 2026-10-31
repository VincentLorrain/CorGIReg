#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeLibrary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "NodeTest/NodeDictionary.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include <fstream>
#include <random>
#include <sstream>

using namespace CorGIReg;
namespace {
using V = NodeValue;
using O = V::Object;
using A = V::Array;
const auto examples = std::filesystem::path(__FILE__).parent_path() / "../../examples/library";

struct Folder {
    std::filesystem::path path;
    Folder() {
        path = std::filesystem::temp_directory_path() / ("corgireg-library-" + std::to_string(std::random_device{}()));
        std::filesystem::create_directories(path);
    }
    ~Folder() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
    std::filesystem::path write(const std::string& name, const std::string& text) const {
        std::filesystem::create_directories((path / name).parent_path());
        std::ofstream(path / name, std::ios::binary) << text;
        return path / name;
    }
};
std::string definition(const std::string& name, const std::string& formats = R"({"json": {"op_types": ["A"]}})") {
    return R"({"schema_version": 1, "name": ")" + name + R"(", "formats": )" + formats + "}";
}
std::string json(const NodeValue& value) {
    std::ostringstream text;
    writeJson(text, value);
    return text.str();
}
std::string openMath(const std::string& version, const std::string& symbol) {
    return R"({"schema_version": )" + version + R"(, "name": "x", "openmath": )" + symbol +
           R"(, "formats": {"json": {"op_types": ["A"]}}})";
}
std::string error(const std::function<void()>& action) {
    try { action(); }
    catch (const std::invalid_argument& failure) { return failure.what(); }
    FAIL("expected invalid_argument");
    return {};
}
}

TEST_CASE("Node library loads the bundled definitions for every format", "[NodeTest][library]") {
    const auto library = NodeLibrary::load({examples / "nodes"});
    const std::map<std::string, std::pair<std::string, std::string>> expected{
        {"conv", {"Conv", "convolution"}}, {"negate", {"Neg", "negate"}}, {"abs", {"Abs", "abs"}},
        {"add", {"Add", "add"}}, {"multiply", {"Mul", "multiply"}}, {"subtract", {"Sub", "subtract"}},
        {"divide", {"Div", "divide"}}, {"exponential", {"Exp", "exponential"}}, {"log", {"Log", "log"}},
        {"sqrt", {"Sqrt", "sqrt"}}, {"tanh", {"Tanh", "tanh"}}};
    REQUIRE(library.definitions().size() == expected.size());
    for (const auto& [name, ops] : expected) {
        const auto* found = library.find(name);
        REQUIRE(found);
        REQUIRE(std::filesystem::path(found->source).is_absolute());
        for (const auto& [format, op] : {std::pair<std::string, std::string>{"onnx", ops.first},
                                         {"stablehlo", "stablehlo." + ops.second}, {"json", ops.first}}) {
            INFO(name << '/' << format);
            const std::map<NodeId, NodeValue> documents{{42, O{{"op", op}}}, {9, O{{"op", "Unrelated"}}}};
            const auto memberships = library.resolve(format, documents);
            for (const auto& [key, ids] : memberships) {
                REQUIRE(ids);
                REQUIRE(*ids == (key == name ? NodeIds{42} : NodeIds{}));
            }
        }
    }
}

TEST_CASE("Bundled definitions state their exact OpenMath symbol", "[NodeTest][library][openmath]") {
    const auto library = NodeLibrary::load({examples / "nodes"});
    const std::map<std::string, std::optional<OpenMathSymbol>> expected{
        {"abs", OpenMathSymbol{"arith1", "abs", ""}}, {"add", OpenMathSymbol{"arith1", "plus", ""}},
        {"subtract", OpenMathSymbol{"arith1", "minus", ""}}, {"multiply", OpenMathSymbol{"arith1", "times", ""}},
        {"divide", OpenMathSymbol{"arith1", "divide", ""}}, {"negate", OpenMathSymbol{"arith1", "unary_minus", ""}},
        {"exponential", OpenMathSymbol{"transc1", "exp", ""}}, {"log", OpenMathSymbol{"transc1", "ln", ""}},
        {"tanh", OpenMathSymbol{"transc1", "tanh", ""}},
        // No exact standard symbol: arith1.root takes a degree, convolution has none.
        {"sqrt", std::nullopt}, {"conv", std::nullopt}};
    REQUIRE(library.definitions().size() == expected.size());
    for (const auto& [name, symbol] : expected) {
        INFO(name);
        const auto* found = library.find(name);
        REQUIRE(found);
        REQUIRE(found->openmath == symbol);
        REQUIRE(found->schemaVersion == (symbol ? 3u : 1u));
    }
}

TEST_CASE("Schema 3 definitions keep conditions and an optional OpenMath symbol", "[NodeTest][library][openmath]") {
    Folder folder;
    const auto path = folder.write("plus.json", R"({"schema_version": 3, "name": "plus", "openmath":
        {"cd": "arith1", "name": "plus", "cdbase": "http://www.openmath.org/cd"},
        "formats": {"json": {"op_types": ["Add"], "where": {"path": ["name"], "regex": "keep.*"}}}})");
    const auto bare = folder.write("bare.json", R"({"schema_version": 3, "name": "bare", "formats": {"json": {"op_types": ["Add"]}}})");
    const auto library = NodeLibrary::load({path, bare});
    const auto* plus = library.find("plus");
    REQUIRE(plus->openmath == OpenMathSymbol{"arith1", "plus", "http://www.openmath.org/cd"});
    REQUIRE(json(plus->openMathDocument()) == R"({"cd":"arith1","cdbase":"http://www.openmath.org/cd","name":"plus"})");
    REQUIRE_FALSE(library.find("bare")->openmath);
    REQUIRE(json(library.find("bare")->openMathDocument()) == "null");
    // The symbol documents meaning only: membership still comes from op_types and where.
    const std::map<NodeId, NodeValue> documents{{0, O{{"op", "Add"}, {"name", "keep_1"}}},
                                                {1, O{{"op", "Add"}, {"name", "drop_1"}}},
                                                {2, O{{"op", "arith1.plus"}, {"name", "keep_2"}}}};
    REQUIRE(*library.resolve("json", documents).at("plus") == NodeIds{0});
    REQUIRE(*library.resolve("json", documents).at("bare") == NodeIds{0, 1});
}

TEST_CASE("Node library applies conditional rules and keeps missing mappings distinct", "[NodeTest][library]") {
    const auto library = NodeLibrary::load({examples / "conditional-nodes"});
    auto conv = [](V kernel, std::string name) {
        return V(O{{"op", "Conv"}, {"name", name}, {"inputs", A{"x", "w"}}, {"attributes", O{{"kernel_shape", kernel}}}});
    };
    const std::map<NodeId, NodeValue> documents{{0, conv(A{3, 3}, "encoder/conv_1")},
                                                {1, conv(A{1, 1}, "encoder/conv_2")},
                                                {2, conv(A{3, 3}, "decoder/conv_3")}};
    REQUIRE(*library.resolve("json", documents).at("conv3x3") == NodeIds{0});
    REQUIRE_FALSE(library.resolve("stablehlo", documents).at("conv3x3"));
    REQUIRE_THROWS_AS(library.generatedOperation("conv3x3", "json"), std::invalid_argument);
    REQUIRE_FALSE(library.generatedOperation("unknown", "json"));
    REQUIRE(*NodeLibrary::load({examples / "nodes"}).generatedOperation("conv", "stablehlo") == "stablehlo.convolution");
}

TEST_CASE("Node library directories are explicit, sorted and not recursive", "[NodeTest][library]") {
    Folder folder;
    folder.write("nodes/b.json", definition("layer"));
    folder.write("nodes/a.json", definition("conv"));
    folder.write("nodes/nested/c.json", definition("hidden"));
    folder.write("nodes/readme.txt", "ignored");
    auto library = NodeLibrary::load({folder.path / "nodes"});
    REQUIRE(library.definitions().size() == 2);
    REQUIRE(library.find("conv")->source == std::filesystem::canonical(folder.path / "nodes/a.json").string());
    REQUIRE_FALSE(library.find("hidden"));
    std::filesystem::create_directories(folder.path / "empty");
    REQUIRE(error([&] { NodeLibrary::load({folder.path / "empty"}); }).find("No node definitions found") != std::string::npos);
    REQUIRE(error([&] { NodeLibrary::load({folder.path / "nodes/readme.txt"}); }).find("must be JSON files") != std::string::npos);
    const auto duplicate = error([&] { NodeLibrary::load({folder.path / "nodes", folder.path / "nodes/a.json"}); });
    REQUIRE(duplicate.find("Duplicate node definition: conv") != std::string::npos);
    REQUIRE(NodeLibrary::load({folder.write("upper.JSON", definition("upper"))}).find("upper"));
}

TEST_CASE("Node definition errors identify the file and definition", "[NodeTest][library]") {
    Folder folder;
    const std::vector<std::pair<std::string, std::string>> cases{
        {"[]", "Invalid node definition fields"},
        {R"({"schema_version": 1, "name": "x", "formats": {"json": {"op_types": ["A"]}}, "extra": 1})", "Invalid node definition fields"},
        {R"({"schema_version": 5, "name": "x", "formats": {"json": {"op_types": ["A"]}}})", "Unsupported node definition schema"},
        {R"({"schema_version": 0, "name": "x", "formats": {"json": {"op_types": ["A"]}}})", "Unsupported node definition schema"},
        {R"({"schema_version": 1.0, "name": "x", "formats": {"json": {"op_types": ["A"]}}})", "Unsupported node definition schema"},
        {R"({"schema_version": true, "name": "x", "formats": {"json": {"op_types": ["A"]}}})", "Unsupported node definition schema"},
        {definition("a-b"), "Invalid predicate name"},
        {R"({"schema_version": 1, "name": "x", "description": 3, "formats": {"json": {"op_types": ["A"]}}})", "Invalid description for x"},
        {definition("x", "{}"), "Node x requires format mappings"},
        {definition("x", R"({"": {"op_types": ["A"]}})"), "Invalid format name for x"},
        {definition("x", R"({"json": {"op_types": []}})"), "op_types must be a nonempty array of strings for x/json"},
        {definition("x", R"({"json": {"op_types": [""]}})"), "op_types must be a nonempty array of strings for x/json"},
        {definition("x", R"({"json": {"op_types": ["A"], "where": {}}})"), "Only op_types rules are supported for x/json"},
        {R"({"schema_version": 2, "name": "x", "formats": {"json": {"op_types": ["A"], "where": []}}})", "where must be a rule object: x/json"},
        {R"({"schema_version": 2, "name": "x", "formats": {"json": {"op_types": ["A"], "other": 1}}})", "Only op_types and where rules are supported for x/json"},
        {"{\"name\": \"x\",}", "JSON line 1, column 14"},
        {openMath("2", R"({"cd": "arith1", "name": "plus"})"), "openmath requires schema_version 3 for x"},
        {openMath("1", R"({"cd": "arith1", "name": "plus"})"), "openmath requires schema_version 3 for x"},
        {openMath("3", R"("arith1.plus")"), "openmath must be an object for x"},
        {openMath("3", R"({"name": "plus"})"), "openmath cd must match"},
        {openMath("3", R"({"cd": "arith1"})"), "openmath name must match"},
        {openMath("3", R"({"cd": "1arith", "name": "plus"})"), "openmath cd must match"},
        {openMath("3", R"({"cd": "arith1", "name": "_plus"})"), "openmath name must match"},
        {openMath("3", R"({"cd": "arith1", "name": "plus-1"})"), "openmath name must match"},
        {openMath("3", R"({"cd": "arith1", "name": 3})"), "openmath name must match"},
        {openMath("3", R"({"cd": "arith1", "name": "plus", "cdbase": ""})"), "openmath cdbase must be a nonempty string"},
        {openMath("3", R"({"cd": "arith1", "name": "plus", "cdbase": 1})"), "openmath cdbase must be a nonempty string"},
        {openMath("3", R"({"cd": "arith1", "name": "plus", "role": "application"})"), "Only cd, name and cdbase are supported in openmath"},
    };
    for (const auto& [text, message] : cases) {
        INFO(text);
        const auto path = folder.write("definition.json", text);
        const auto what = error([&] { NodeLibrary::load({path}); });
        REQUIRE(what.find(std::filesystem::canonical(path).string() + ": ") == 0);
        REQUIRE(what.find(message) != std::string::npos);
    }
    const auto invalidRule = folder.write("rule.json",
        R"({"schema_version": 2, "name": "bad", "formats": {"json": {"op_types": ["Missing"], "where": {"path": ["x"], "exists": 1}}, "onnx": {"op_types": ["A"]}}})");
    const auto library = NodeLibrary::load({invalidRule});
    const auto what = error([&] { library.resolve("json", {}); });
    REQUIRE(what.find(std::filesystem::canonical(invalidRule).string() + " (bad): where") == 0);
    REQUIRE_NOTHROW(library.resolve("onnx", {}));
}

TEST_CASE("Node library predicates drive matching without the global dictionary", "[NodeTest][library][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    std::map<NodeId, NodeValue> documents;
    const std::vector<std::string> ops{"stablehlo.convolution", "stablehlo.convolution", "stablehlo.negate", "Other"};
    for (NodeId id = 0; id < ops.size(); ++id) {
        graph->addNode(id, "raw_" + std::to_string(id), "n" + std::to_string(id));
        documents.emplace(id, O{{"op", ops[id]}});
    }
    graph->addEdge(0, 1); graph->addEdge(1, 2); graph->addEdge(2, 3);
    const auto library = NodeLibrary::load({examples / "nodes"});
    NodePredicates predicates;
    addDefinitionPredicates(predicates, library.resolve("stablehlo", documents));
    auto result = PGInterpreterTG("conv+->negate").interpret()->test(graph, {0}, predicates);
    REQUIRE(result->getMatches().size() == 1);
    REQUIRE(result->getMatches()[0]->getAll() == NodeIds{0, 1, 2});
    REQUIRE(result->getMatches()[0]->getWalks().at(0).quantifiers.at(0).count() == 2);
    REQUIRE_FALSE(PGInterpreterTG("conv->negate->$").interpret()->test(graph, {1}, predicates)->isMatch());
    REQUIRE_FALSE(NodeDictionary::hasFunction("conv"));
}
