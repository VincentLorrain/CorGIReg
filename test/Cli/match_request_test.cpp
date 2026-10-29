#include <catch2/catch_test_macros.hpp>
#include "Cli/MatchRequest.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <sstream>

using namespace CorGIReg;

TEST_CASE("Typed matching protocol rejects malformed document values", "[cli][rules]") {
    for (const auto* value : {"b 2", "i -", "u -1", "d nan", "d 1x", "a 1000001",
                              "o 2 78 n 78 n", "unknown"}) {
        std::istringstream input(std::string("CORGiReg3 1 0 41 61 ") + value + " 0 0 41 100");
        std::ostringstream output;
        REQUIRE_THROWS(matchRequest(input, output));
        REQUIRE(output.str().empty());
    }
}

TEST_CASE("Imported operation graphs preserve IDs names and reciprocal edges", "[cli]") {
    ImportedGraph graph;
    graph.addNode(42, "A", "first"); graph.addNode(0, "B", "last");
    graph.addEdge(42, 0); graph.addEdge(42, 0);
    REQUIRE(graph.children(42) == NodeIds{0});
    REQUIRE(graph.parents(0) == NodeIds{42});
    REQUIRE(graph.nodeName(42) == "first");
    REQUIRE_THROWS_AS(graph.addNode(0, "A", "duplicate"), std::invalid_argument);
    REQUIRE_THROWS_AS(graph.addEdge(42, 99), std::out_of_range);
    REQUIRE(graph.children(42) == NodeIds{0});
}

TEST_CASE("Matching requests search source nodes and serialize quantifiers", "[cli]") {
    // A+->B on 42 -> 0 -> 9. Names include a quote and newline.
    std::istringstream input("CORGiReg1 3 42 41 61220a 0 41 62 9 42 63 2 42 0 0 9 412b2d3e42 100");
    std::ostringstream output;
    matchRequest(input, output);
    REQUIRE(output.str().find("\"query\":\"A+->B\"") != std::string::npos);
    REQUIRE(output.str().find("\"starts\":[42]") != std::string::npos);
    REQUIRE(output.str().find("\"count\":2") != std::string::npos);
    REQUIRE(output.str().find("\"iteration_ends\":[1,2]") != std::string::npos);
    REQUIRE(output.str().find("a\\\"\\u000a") != std::string::npos);
    REQUIRE(output.str().find("\"node_ids\":[0,9,42]") != std::string::npos);
}

TEST_CASE("Matching protocol rejects invalid requests before search", "[cli]") {
    for (const auto* request : {
        "bad", "CORGiReg1", "CORGiReg1 -1", "CORGiReg1 1 0 zz 61",
        "CORGiReg1 2 0 41 61 0 42 62 0 41 100",
        "CORGiReg1 1 0 41 61 1 0 99 41 100",
        "CORGiReg1 0 0 - 100", "CORGiReg1 0 0 41 0",
        "CORGiReg1 0 0 41 100 unexpected",
        "CORGiReg1 2 0 41 61 1 41 62 0 41 1"}) {
        INFO(request);
        std::istringstream input(request); std::ostringstream output;
        REQUIRE_THROWS(matchRequest(input, output));
    }
    std::istringstream noMatch("CORGiReg1 1 0 41 61 0 42 100");
    std::ostringstream out;
    matchRequest(noMatch, out);
    REQUIRE(out.str() == "{\"query\":\"B\",\"matches\":[]}\n");
}

TEST_CASE("Request-scoped library predicates preserve semantic capture names", "[cli]") {
    // Raw C -> R, semantic conv -> relu; also expose C as the predicate layer.
    const std::string prefix = "CORGiReg2 2 42 43 63 7 52 72 1 42 7 3 "
        "636f6e76 1 1 42 72656c75 1 1 7 6c61796572 1 1 42 ";
    for (const auto* query : {"636f6e762d3e72656c75", "6c617965722d3e72656c75"}) {
        std::istringstream input(prefix + query + " 100");
        std::ostringstream output;
        matchRequest(input, output);
        REQUIRE(output.str().find("\"node_ids\":[7,42]") != std::string::npos);
        REQUIRE(output.str().find("\"predicate\":\"relu\"") != std::string::npos);
    }
    // The alias must not leak into a subsequent request.
    std::istringstream next("CORGiReg1 1 42 43 63 0 636f6e76 100");
    std::ostringstream output;
    matchRequest(next, output);
    REQUIRE(output.str().find("\"matches\":[]") != std::string::npos);
}

TEST_CASE("Node library protocol validates memberships and unavailable definitions", "[cli]") {
    const std::string prefix = "CORGiReg2 1 42 43 63 0 ";
    for (const auto* definitions : {"1 636f6e76 1 1 99", "1 636f6e76 2",
                                   "2 636f6e76 0 636f6e76 0", "1 2e 0"}) {
        std::istringstream input(prefix + definitions + " 636f6e76 100");
        std::ostringstream output;
        REQUIRE_THROWS(matchRequest(input, output));
        REQUIRE(output.str().empty());
    }
    std::istringstream unavailable(prefix + "1 636f6e76 0 636f6e76 100");
    std::ostringstream unused;
    REQUIRE_THROWS_AS(matchRequest(unavailable, unused), std::invalid_argument);
    // Missing mappings are allowed for definitions not used by the query.
    std::istringstream other(prefix + "1 636f6e76 0 43 100");
    matchRequest(other, unused);
    REQUIRE(unused.str().find("\"node_ids\":[42]") != std::string::npos);
}

TEST_CASE("Native node libraries resolve request documents and report definitions", "[cli][library]") {
    const auto library = NodeLibrary::load({std::filesystem::path(__FILE__).parent_path() / "../../examples/library/nodes"});
    // Conv(0) -> Neg(1); documents carry the original operation names.
    const std::string graph = " 2 0 436f6e76 61 o 1 6f70 s 436f6e76 1 4e6567 62 o 1 6f70 s 4e6567 1 0 1 0 ";
    std::istringstream input("CORGiReg3" + graph + "636f6e762d3e6e6567617465 100");
    std::ostringstream output;
    matchRequest(input, output, 1, &library, "onnx");
    const auto text = output.str();
    REQUIRE(text.find("\"node_ids\":[0,1]") != std::string::npos);
    REQUIRE(text.find("\"definition_nodes\"") == std::string::npos);
    REQUIRE(text.find("\"conv\":{\"description\":") != std::string::npos);
    REQUIRE(text.find("\"format\":\"onnx\",\"rule\":{\"op_types\":[\"Conv\"]},\"node_ids\":[0]}") != std::string::npos);
    REQUIRE(text.find("\"negate\":{") != std::string::npos);

    std::istringstream unmapped("CORGiReg3" + graph + "636f6e762d3e6e6567617465 100");
    std::ostringstream ignored;
    const auto conditional = NodeLibrary::load({std::filesystem::path(__FILE__).parent_path() / "../../examples/library/conditional-nodes"});
    REQUIRE_THROWS_AS(matchRequest(unmapped, ignored, 1, &library, "openvx"), std::invalid_argument);
    for (const std::string request : {"CORGiReg1 1 0 41 61 0 41 100",
                                      "CORGiReg3 1 0 41 61 o 0 0 1 41 0 41 100"}) {
        INFO(request);
        std::istringstream rejected(request);
        REQUIRE_THROWS_AS(matchRequest(rejected, ignored, 1, &library, "json"), std::invalid_argument);
    }
    std::ostringstream listing;
    writeNodeLibrary(listing, conditional);
    REQUIRE(listing.str().rfind("{\"conv3x3\":{\"description\":", 0) == 0);
    REQUIRE(listing.str().find("\"schema_version\":2") != std::string::npos);
}
