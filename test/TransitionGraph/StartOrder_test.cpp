#include <catch2/catch_test_macros.hpp>
#include "Cli/MatchRequest.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "TransitionGraph/State.hpp"
#include <array>
#include <random>
#include <sstream>

using namespace CorGIReg;

namespace {
// Leaves State-sized holes at random positions so the next compilation's
// states land in a different address order than the previous one.
struct HeapNoise {
    std::vector<std::shared_ptr<std::array<char, sizeof(State)>>> kept;
    void shuffle(std::mt19937& rng) {
        std::vector<std::shared_ptr<std::array<char, sizeof(State)>>> blocks(64 + rng() % 64);
        for (auto& block : blocks) block = std::make_shared<std::array<char, sizeof(State)>>();
        for (auto& block : blocks) if (rng() % 2) kept.push_back(std::move(block));
    }
};
}

TEST_CASE("Start planning does not depend on state addresses", "[start][determinism]") {
    // 0:A 1:B 2:C 3:D 4:E 5:F
    auto graph = std::make_shared<ImportedGraph>();
    const std::vector<std::string> types{"A", "B", "C", "D", "E", "F"};
    NodePredicates predicates;
    for (NodeId id = 0; id < types.size(); ++id) {
        graph->addNode(id, types[id], "n");
        predicates[types[id]] = [graph, key = types[id]](NodeId node) { return graph->getNodes().at(node).predicate == key; };
    }
    const std::vector<NodeId> nodes{0, 1, 2, 3, 4, 5};
    using Candidates = std::vector<std::vector<std::optional<NodeId>>>;

    const unsigned seed = 20261030;
    std::mt19937 rng(seed);
    HeapNoise noise;
    for (const std::string query : {"A->B;C->D;E->F", "A#->B;C->A#", "F<-E;A->B"}) {
        auto reference = PGInterpreterTG(query).interpret()->startCandidates(nodes, predicates);
        for (int run = 0; run < 50; ++run) {
            INFO("seed " << seed << " query " << query << " run " << run);
            noise.shuffle(rng);
            auto pattern = PGInterpreterTG(query).interpret();
            REQUIRE(pattern->startCandidates(nodes, predicates) == reference);
        }
    }
}

TEST_CASE("Match report order does not depend on state addresses", "[start][determinism][cli]") {
    // A->B twice and C->D twice: four start combinations whose enumeration
    // order depends on which path's start group comes first.
    // 0:A->1:B  2:C->3:D  4:A->5:B  6:C->7:D, query "A->B;C->D".
    const std::string request =
        "CORGiReg1 8 0 41 - 1 42 - 2 43 - 3 44 - 4 41 - 5 42 - 6 43 - 7 44 - "
        "4 0 1 2 3 4 5 6 7 412d3e423b432d3e44 100";
    auto run = [&] {
        std::istringstream input(request);
        std::ostringstream output;
        matchRequest(input, output);
        return output.str();
    };
    const auto reference = run();
    REQUIRE(reference.find("\"starts\"") != std::string::npos);

    const unsigned seed = 20261030;
    std::mt19937 rng(seed);
    HeapNoise noise;
    for (int attempt = 0; attempt < 50; ++attempt) {
        INFO("seed " << seed << " attempt " << attempt);
        noise.shuffle(rng);
        REQUIRE(run() == reference);
    }
}
