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

// Generation rejects patterns whose boundaries cannot hold with the drawn
// counts, such as A#->$;A#->B; any other generation error fails the test.
std::optional<Case> generated(std::uint64_t seed, const std::string& query, const Settings& settings,
                              std::vector<std::string> library = {}, std::string format = "json") {
    try { return Case::generate(seed, query, settings, std::move(library), std::move(format)); }
    catch (const std::length_error&) { return std::nullopt; } // Exceeds maxNodes.
    catch (const std::invalid_argument& error) {
        if (std::string(error.what()).rfind("Unsatisfiable boundary", 0) == 0) return std::nullopt;
        throw;
    }
}

std::optional<Case> randomCase(std::uint64_t seed, const Settings& settings,
                               std::vector<std::string> library = {}, std::string format = "json") {
    RandomExpressionGenerator::setSeed(static_cast<std::uint32_t>(seed));
    const auto query = RandomExpressionGenerator::allExpr(static_cast<unsigned>(seed % (settings.depth + 1)));
    if (PatternInfo(query).nestedInAnchoredRepetition()) return std::nullopt;
    return generated(seed, query, settings, std::move(library), std::move(format));
}
}

TEST_CASE("Seeded random patterns match the graphs generated from them", "[generation-matching][random][app]") {
    const Dictionary dictionary({"GmA", "GmB", "GmC", "GmD"});
    const auto settings = Settings::fromEnvironment({});
    std::uint64_t checked = 0, ran = 0;
    for (std::uint64_t i = 0; i < settings.iterations && !settings.exhausted(); ++i, ++ran) {
        auto item = randomCase(settings.seed + i, settings);
        if (item && check(*item, "match")) ++checked;
    }
    REQUIRE(checked * 4 >= ran);
    REQUIRE(checked > 0);
}

TEST_CASE("Library keys generate source operations that match through the same library", "[generation-matching][library][app]") {
    const Dictionary dictionary({"conv", "negate", "add", "tanh"});
    const auto settings = Settings::fromEnvironment({});
    const std::vector<std::string> library{(examples / "library/nodes").string()};
    std::uint64_t checked = 0, ran = 0;
    for (std::uint64_t i = 0; i < settings.iterations && !settings.exhausted(); ++i, ++ran) {
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
    REQUIRE(checked * 4 >= ran);
    REQUIRE(checked > 0);
}

TEST_CASE("Match-preserving embedding and enrichment keep the generated subgraph", "[generation-matching][random][app]") {
    const Dictionary dictionary({"GmA", "GmB", "GmC", "GmD"});
    const auto settings = Settings::fromEnvironment({});
    std::uint64_t checked = 0, ran = 0;
    for (std::uint64_t i = 0; i < settings.iterations && !settings.exhausted(); ++i, ++ran) {
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
    REQUIRE(checked * 4 >= ran);
    REQUIRE(checked > 0);
}

TEST_CASE("Mutations of distinct mandatory patterns are classified and rejected", "[generation-matching][negative][random][app]") {
    const Dictionary dictionary({"NgA", "NgB", "NgC", "NgD", "NgE", "NgF", "NgG", "NgH", "NgI", "NgJ"});
    auto settings = Settings::fromEnvironment({});
    std::uint64_t accepted = 0, rejected = 0, attempts = 0;
    for (std::uint64_t seed = settings.seed; accepted < settings.iterations && attempts < 100 * settings.iterations &&
                                             !settings.exhausted(); ++seed, ++attempts) {
        RandomExpressionGenerator::setSeed(static_cast<std::uint32_t>(seed));
        const auto query = RandomExpressionGenerator::allExpr(0);
        if (!PatternInfo(query).distinctMandatory()) continue;
        const auto candidate = generated(seed, query, settings);
        if (!candidate || !check(*candidate, "match")) continue;
        const auto& base = *candidate;
        ++accepted;
        // Every element is required and owns the only nodes of its operation.
        const auto all = search(base);
        for (const auto& match : all.matches) {
            INFO(describe(base));
            REQUIRE(match->getAll() == base.generated.ids());
        }
        std::set<std::pair<NodeId, NodeId>> pairs;
        for (const auto& edge : base.generated.edges) pairs.emplace(edge.from, edge.to);
        for (const auto& [from, to] : pairs) {
            auto item = base;
            removeEdge(item, from, to);
            rejected += check(item, "no-match");
        }
        for (const auto& [id, node] : base.generated.nodes) {
            auto item = base;
            retype(item, id);
            rejected += check(item, "no-match");
            item = base;
            const auto parent = item.acceptsParent(id);
            addParent(item, id);
            rejected += check(item, parent ? "match" : "no-match") && !parent;
            item = base;
            const auto child = item.acceptsChild(id);
            addChild(item, id);
            rejected += check(item, child ? "match" : "no-match") && !child;
        }
    }
    REQUIRE((accepted == settings.iterations || settings.exhausted()));
    REQUIRE(rejected > accepted);
}

TEST_CASE("Hand-specified mutations of generated graphs check ports boundaries and identities", "[generation-matching][negative][app]") {
    const Settings settings;
    SECTION("numbered ports") {
        auto item = Case::generate(0, "PortA-2->PortB", settings);
        REQUIRE(item.generated.edges.size() == 1);
        REQUIRE(item.generated.edges[0].ports.input == PortNumber{2});
        REQUIRE(check(item, "match"));
        REQUIRE(check(item.withQuery("PortA->PortB"), "match"));
        REQUIRE(check(item.withQuery("PortA-*->PortB"), "match"));
        REQUIRE(check(item.withQuery("PortA-1->PortB"), "no-match"));
        for (const std::optional<PortNumber> port : {std::optional<PortNumber>{1}, std::optional<PortNumber>{}}) {
            auto mutated = item;
            setInputPort(mutated, 0, 1, port);
            REQUIRE(check(mutated, "no-match"));
            REQUIRE(check(mutated.withQuery("PortA-*->PortB"), "match"));
        }
    }
    SECTION("numbered identities") {
        auto item = Case::generate(0, "IdA#1->IdB;IdA#1->IdC", settings);
        REQUIRE(item.generated.nodes.size() == 3);
        REQUIRE(check(item, "match"));
        split(item, 0, {2});
        REQUIRE(check(item, "no-match"));
        auto separate = item.withQuery("IdA#1->IdB;IdA#2->IdC");
        separate.generated = separate.graph;
        REQUIRE(check(separate, "match"));
    }
    SECTION("boundaries and plain degrees") {
        const auto item = Case::generate(0, "BdA->BdB->$", settings);
        REQUIRE(check(item, "match"));
        auto mutated = item;
        addChild(mutated, 1);
        REQUIRE(check(mutated, "no-match"));                             // B is no longer terminal.
        REQUIRE(check(mutated.withQuery("BdA->BdB"), "match"));          // Plain B: at most one child.
        addChild(mutated, 1);
        REQUIRE(check(mutated.withQuery("BdA->BdB"), "no-match"));
        REQUIRE(check(mutated.withQuery("BdA->BdB#"), "match"));
        mutated = item;
        addParent(mutated, 0);
        REQUIRE(check(mutated, "match"));
        addParent(mutated, 0);
        REQUIRE(check(mutated, "no-match"));
    }
    SECTION("strict sides") {
        const auto item = Case::generate(0, "StP->$StH$->StC", settings);
        for (const auto& [side, query] : std::vector<std::pair<int, std::string>>{
                 {0, "StP->StH#->StC"}, {1, "StP->$StH->StC"}, {2, "StP->StH$->StC"}, {3, "StP->$StH$->StC"}}) {
            INFO(query);
            auto parent = item.withQuery(query), child = item.withQuery(query);
            addParent(parent, 1);
            addChild(child, 1);
            REQUIRE(check(parent, side & 1 ? "no-match" : "match"));
            REQUIRE(check(child, side & 2 ? "no-match" : "match"));
        }
    }
}

TEST_CASE("Failure artifacts replay the same graph pattern and matches", "[generation-matching][replay][app]") {
    const Dictionary dictionary({"GmA", "GmB", "GmC", "GmD"});
    const Settings settings;
    std::uint64_t replayed = 0;
    for (std::uint64_t seed = settings.seed; replayed < 5; ++seed) {
        auto item = randomCase(seed, settings, {(examples / "library/nodes").string()});
        if (!item || search(*item, true).truncated) continue;
        std::mt19937 random(static_cast<std::uint32_t>(seed));
        embed(*item, random);
        addDisconnected(*item, random, 2, 2);
        const auto original = search(*item, true);
        std::ostringstream text;
        writeJson(text, caseJson(*item, "match", original));
        const auto copy = caseFromJson(parseJson(text.str()));
        INFO(text.str());
        REQUIRE(copy.query == item->query);
        REQUIRE(copy.repetitions == item->repetitions);
        REQUIRE(copy.mutations == item->mutations);
        std::ostringstream before, after;
        writeJson(before, item->graph.json());
        writeJson(after, copy.graph.json());
        REQUIRE(before.str() == after.str());
        REQUIRE(signatures(search(copy, true)) == signatures(original));
        ++replayed;
    }
}

// Replays one saved artifact: CORGIREG_GM_REPLAY=path tests "[replay-file]"
TEST_CASE("Replay a saved generation-matching artifact", "[.][replay-file]") {
    const char* path = std::getenv("CORGIREG_GM_REPLAY");
    REQUIRE(path);
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    std::ostringstream text;
    text << input.rdbuf();
    const auto document = parseJson(text.str());
    const auto item = caseFromJson(document);
    check(item, std::get<std::string>(std::get<O>(document.data).at("expectation").data));
}
