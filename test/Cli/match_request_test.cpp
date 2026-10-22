#include <catch2/catch_test_macros.hpp>
#include "Cli/MatchRequest.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <sstream>

using namespace CorGIReg;

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
