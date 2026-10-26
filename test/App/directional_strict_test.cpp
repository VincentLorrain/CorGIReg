#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <random>
using namespace CorGIReg;

namespace {
NodePredicates conditions(const std::shared_ptr<ImportedGraph>& graph) {
    NodePredicates result;
    for (const auto& entry : graph->getNodes()) {
        const auto key = entry.second.predicate;
        result[key] = [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; };
    }
    return result;
}

std::string anchor(unsigned sides);

TEST_CASE("Seeded random graphs check directional coverage against independent neighbor sets", "[directional][random][app]") {
    constexpr unsigned seed = 20261026;
    std::mt19937 random(seed);
    for (unsigned trial = 0; trial < 200; ++trial) {
        auto graph = std::make_shared<ImportedGraph>();
        graph->addNode(0, "DirB", "hub");
        graph->addNode(1, "DirA", "parent"); graph->addNode(2, "DirC", "child");
        const bool parentLink = random() % 5 != 0, childLink = random() % 5 != 0;
        NodeIds expectedParents, expectedChildren;
        if (parentLink) { graph->addEdge(1, 0); expectedParents.insert(1); }
        if (childLink) { graph->addEdge(0, 2); expectedChildren.insert(2); }
        const unsigned incoming = random() % 4, outgoing = random() % 4;
        for (unsigned i = 0; i < incoming; ++i) {
            graph->addNode(100 + i, "Other", "extra parent");
            graph->addEdge(100 + i, 0); expectedParents.insert(100 + i);
            graph->addEdge(100 + i, 100 + i); // Irrelevant cycle must not alter coverage.
        }
        for (unsigned i = 0; i < outgoing; ++i) {
            graph->addNode(200 + i, "Other", "extra child");
            graph->addEdge(0, 200 + i); expectedChildren.insert(200 + i);
        }
        for (unsigned sides = 0; sides < 4; ++sides) {
            const bool expected = parentLink && childLink &&
                (!(sides & 1) || expectedParents == NodeIds{1}) &&
                (!(sides & 2) || expectedChildren == NodeIds{2});
            const auto key = anchor(sides);
            for (const auto& query : {"DirA->" + key + "->DirC->$", "DirC<-" + key + "<-DirA"}) {
                INFO("seed=" << seed << ", case=" << trial << ", query=" << query
                     << ", incoming=" << incoming << ", outgoing=" << outgoing
                     << ", base edges=" << parentLink << ',' << childLink);
                const auto result = PGInterpreterTG(query).interpret()->test(graph, {1}, conditions(graph));
                REQUIRE(result->isMatch() == expected);
                for (const auto& match : result->getMatches()) REQUIRE(match->getAll() == NodeIds{0, 1, 2});
            }
        }
    }
}
std::string anchor(unsigned sides) {
    return std::string(sides & 1 ? "$" : "") + "DirB" + (sides & 2 ? "$" : sides & 1 ? "" : "#");
}
}

TEST_CASE("Directional strict nodes cover exactly the selected graph sides", "[directional][app]") {
    for (unsigned extra = 0; extra < 4; ++extra) {
        auto graph = std::make_shared<ImportedGraph>();
        graph->addNode(0, "DirB", "hub");
        graph->addNode(1, "DirA", "parent"); graph->addNode(2, "DirC", "child");
        graph->addNode(3, "Extra", "extra parent"); graph->addNode(4, "Extra", "extra child");
        graph->addEdge(1, 0); graph->addEdge(0, 2);
        if (extra & 1) graph->addEdge(3, 0);
        if (extra & 2) graph->addEdge(0, 4);
        for (unsigned sides = 0; sides < 4; ++sides) {
            const auto key = anchor(sides);
            for (const auto& query : {"DirA->" + key + "->DirC", "DirC<-" + key + "<-DirA"}) {
                INFO("query=" << query << ", extra sides=" << extra);
                auto result = PGInterpreterTG(query).interpret()->test(graph, {1}, conditions(graph));
                REQUIRE(result->isMatch() == ((sides & extra) == 0));
                for (const auto& match : result->getMatches()) {
                    REQUIRE(match->getAll() == NodeIds{0, 1, 2});
                    REQUIRE(match->getQuery() == query);
                    REQUIRE(match->getWalks().at(0).nodes.at(1).predicate == "DirB");
                }
            }
        }
    }
}

TEST_CASE("Directional coverage includes a single neighbor and ignores traversal reversal", "[directional][app]") {
    for (unsigned edges = 0; edges < 4; ++edges) {
        auto graph = std::make_shared<ImportedGraph>();
        graph->addNode(0, "DirB", "hub");
        graph->addNode(1, "Other", "parent"); graph->addNode(2, "Other", "child");
        if (edges & 1) graph->addEdge(1, 0);
        if (edges & 2) graph->addEdge(0, 2);
        for (unsigned sides = 0; sides < 4; ++sides) {
            auto pattern = PGInterpreterTG(anchor(sides)).interpret();
            for (unsigned reverse = 0; reverse < 2; ++reverse) {
                INFO("sides=" << sides << ", edges=" << edges << ", reverse=" << reverse);
                if (reverse) for (const auto& transition : pattern->getTransitions()) transition->flipDir();
                auto result = pattern->test(graph, {0}, conditions(graph));
                REQUIRE(result->isMatch() == ((sides & edges) == 0));
                for (const auto& match : result->getMatches()) REQUIRE(match->getAll() == NodeIds{0});
            }
        }
    }
}

TEST_CASE("Directional anchors share numbered identity across different strict occurrences", "[directional][app]") {
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(0, "DirB", "hub");
    graph->addNode(1, "DirA", "parent"); graph->addNode(2, "DirC", "child");
    graph->addEdge(1, 0); graph->addEdge(0, 2);
    for (const std::string query : {"DirA->$DirB#01;DirB$1->DirC", "DirA->$DirB$1;DirB#01->DirC"}) {
        auto pattern = PGInterpreterTG(query).interpret();
        bool found = false;
        for (const auto& starts : {std::vector<std::optional<NodeId>>{1, 0}, {0, 1}}) {
            auto result = pattern->test(graph, starts, conditions(graph));
            found |= result->isMatch();
            for (const auto& match : result->getMatches()) REQUIRE(match->getAll() == NodeIds{0, 1, 2});
        }
        REQUIRE(found);
    }
    REQUIRE(PGInterpreterTG("DirA->$DirB$->DirC->$").interpret()->test(graph, {1}, conditions(graph))->isMatch());
    REQUIRE_FALSE(PGInterpreterTG("DirA->$DirB$->$").interpret()->test(graph, {1}, conditions(graph))->isMatch());
}
