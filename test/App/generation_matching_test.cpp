#include "GenerationMatching.hpp"
#include "Grexgex/RandomExpressionGenerator.h"

using namespace CorGIReg;
using namespace CorGIReg::GenerationMatching;

namespace {
const auto examples = std::filesystem::path(__FILE__).parent_path() / "../../examples";

struct Dictionary {
    explicit Dictionary(const std::vector<std::string>& words) { RandomExpressionGenerator::setDictionary(words); }
    ~Dictionary() { RandomExpressionGenerator::clearDictionary(); }
};

// Boundary assertions are not generated (END emits no node), so patterns using
// them are not valid generator oracles; explicit tests cover them instead.
std::optional<Case> randomCase(std::uint64_t seed, const Settings& settings,
                               std::vector<std::string> library = {}, std::string format = "json") {
    RandomExpressionGenerator::setSeed(static_cast<std::uint32_t>(seed));
    const auto query = RandomExpressionGenerator::allExpr(static_cast<unsigned>(seed % (settings.depth + 1)));
    const PatternInfo pattern(query);
    if (pattern.boundary || pattern.nestedInAnchoredRepetition()) return std::nullopt;
    try { return Case::generate(seed, query, settings, std::move(library), std::move(format)); }
    catch (const std::length_error&) { return std::nullopt; } // Exceeds maxNodes.
}
}

TEST_CASE("Seeded random patterns match the graphs generated from them", "[generation-matching][random][app]") {
    const Dictionary dictionary({"GmA", "GmB", "GmC", "GmD"});
    const auto settings = Settings::fromEnvironment({});
    std::uint64_t checked = 0;
    for (std::uint64_t i = 0; i < settings.iterations; ++i) {
        auto item = randomCase(settings.seed + i, settings);
        if (item && check(*item, "match")) ++checked;
    }
    REQUIRE(checked * 4 >= settings.iterations);
}

TEST_CASE("Library keys generate source operations that match through the same library", "[generation-matching][library][app]") {
    const Dictionary dictionary({"conv", "negate", "add", "tanh"});
    const auto settings = Settings::fromEnvironment({});
    const std::vector<std::string> library{(examples / "library/nodes").string()};
    std::uint64_t checked = 0;
    for (std::uint64_t i = 0; i < settings.iterations; ++i) {
        const std::string format = std::vector<std::string>{"json", "onnx", "stablehlo"}[i % 3];
        auto item = randomCase(settings.seed + i, settings, library, format);
        if (!item) continue;
        for (const auto& [id, node] : item->generated.nodes) {
            INFO(describe(*item));
            const auto operations = item->library().find(node.predicate)->operations(format);
            REQUIRE(node.op == operations.front());
        }
        if (check(*item, "match")) ++checked;
    }
    REQUIRE(checked * 4 >= settings.iterations);
}

TEST_CASE("Match-preserving embedding and enrichment keep the generated subgraph", "[generation-matching][random][app]") {
    const Dictionary dictionary({"GmA", "GmB", "GmC", "GmD"});
    const auto settings = Settings::fromEnvironment({});
    std::uint64_t checked = 0;
    for (std::uint64_t i = 0; i < settings.iterations; ++i) {
        auto item = randomCase(settings.seed + i, settings);
        if (!item || !check(*item, "match")) continue;
        std::mt19937 random(static_cast<std::uint32_t>(item->seed));
        const auto before = search(*item, true);

        addDisconnected(*item, random, settings.extraNodes, settings.extraEdges);
        {
            // Disconnected components cannot change matches from generated starts.
            INFO(describe(*item));
            REQUIRE(signatures(search(*item, true)) == signatures(before));
        }
        embed(*item, random);
        REQUIRE(check(*item, "match"));
        addBranches(*item, random, settings.extraNodes);
        linkAnchors(*item, random, settings.extraEdges);
        REQUIRE(check(*item, "match"));
        ++checked;
    }
    REQUIRE(checked * 4 >= settings.iterations);
}
