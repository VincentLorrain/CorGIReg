#include <catch2/catch_test_macros.hpp>
#include "NodeTest/NodeDictionary.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <regex>

using namespace CorGIReg;

TEST_CASE("Repetition selects its own common anchor after other compilations", "[regression]") {
    for (const std::string anchorKey : {"OrderAnchorFirst", "OrderAnchorSecond"}) {
        const auto leafKey = anchorKey + "Leaf";
        REQUIRE(NodeDictionary::registerFunction(anchorKey, [anchorKey](NodeNN::Ptr node) {
            return node->getType() == anchorKey;
        }));
        REQUIRE(NodeDictionary::registerFunction(leafKey, [leafKey](NodeNN::Ptr node) {
            return node->getType() == leafKey;
        }));
        GraphNN graph;
        auto anchor = std::make_shared<NodeNN>();
        anchor->setInfo("Type", anchorKey);
        auto first = std::make_shared<NodeNN>();
        first->setInfo("Type", leafKey);
        auto second = std::make_shared<NodeNN>();
        second->setInfo("Type", leafKey);
        graph.addEdge(anchor, first);
        graph.addEdge(anchor, second);
        auto outgoing = PGInterpreterTG("($" + anchorKey + "$->" + leafKey + ")+").interpret();
        REQUIRE(outgoing->test({anchor})->isMatch());
        anchor->removeChild(first); first->removeParent(anchor);
        anchor->removeChild(second); second->removeParent(anchor);
        graph.addEdge(first, anchor);
        graph.addEdge(second, anchor);
        auto incoming = PGInterpreterTG("(" + leafKey + "->$" + anchorKey + "$)+").interpret();
        REQUIRE(incoming->test({first})->isMatch());
        REQUIRE_FALSE(incoming->test({anchor})->isMatch());
        first->removeChild(anchor); anchor->removeParent(first);
        second->removeChild(anchor); anchor->removeParent(second);
    }
}

TEST_CASE("Distinct numbered identities reject a node already bound to another one", "[regression][app]") {
    // D0 -> A1 -> C2 -> D0 and D0 -> A3. Walking back into D0 reaches A1 again as A#2.
    auto graph = std::make_shared<ImportedGraph>();
    graph->addNode(0, "D", "d"); graph->addNode(1, "A", "a1"); graph->addNode(2, "C", "c"); graph->addNode(3, "A", "a3");
    graph->addEdge(0, 1); graph->addEdge(1, 2); graph->addEdge(2, 0); graph->addEdge(0, 3);
    auto predicatesOf = [](const std::shared_ptr<ImportedGraph>& source) {
        NodePredicates result;
        for (const std::string key : {"A", "C", "D"})
            result[key] = [source, key](NodeId id) { return source->getNodes().at(id).predicate == key; };
        return result;
    };
    const auto predicates = predicatesOf(graph);
    auto pattern = PGInterpreterTG("D#->A#1->C#->D#->A#2").interpret();
    std::set<NodeIds> found;
    for (NodeId start = 0; start < 4; ++start) {
        const auto result = pattern->test(graph, {start}, predicates);
        for (const auto& match : result->getMatches()) found.insert(match->getAll());
    }
    REQUIRE(found == std::set<NodeIds>{{0, 1, 2, 3}});
    // Without A3 the only remaining binding of A#2 is A1, which is rejected.
    auto cycle = std::make_shared<ImportedGraph>();
    cycle->addNode(0, "D", "d"); cycle->addNode(1, "A", "a1"); cycle->addNode(2, "C", "c");
    cycle->addEdge(0, 1); cycle->addEdge(1, 2); cycle->addEdge(2, 0);
    for (NodeId start = 0; start < 3; ++start)
        REQUIRE_FALSE(pattern->test(cycle, {start}, predicatesOf(cycle))->isMatch());
}

namespace {
std::shared_ptr<ImportedGraph> anchorGraph(const std::vector<std::string>& types,
                                           const std::vector<std::pair<NodeId, NodeId>>& edges) {
    auto graph = std::make_shared<ImportedGraph>();
    for (NodeId id = 0; id < types.size(); ++id) graph->addNode(id, types[id], types[id] + std::to_string(id));
    for (const auto& [from, to] : edges) graph->addEdge(from, to);
    return graph;
}
std::vector<std::shared_ptr<MatchSolution>> allMatches(const std::string& query, const std::shared_ptr<ImportedGraph>& graph) {
    NodePredicates predicates;
    for (const auto& entry : graph->getNodes()) {
        const auto key = entry.second.predicate;
        predicates[key] = [graph, key](NodeId id) { return graph->getNodes().at(id).predicate == key; };
    }
    std::vector<std::shared_ptr<MatchSolution>> result;
    auto pattern = PGInterpreterTG(query).interpret();
    for (const auto& entry : graph->getNodes()) {
        const auto found = pattern->test(graph, {entry.first}, predicates);
        result.insert(result.end(), found->getMatches().begin(), found->getMatches().end());
    }
    return result;
}
}

TEST_CASE("Brace quantifiers enforce inclusive bounds on walks and anchors", "[app][repetition-range]") {
    const auto chain = anchorGraph({"A", "A", "A", "A"}, {{0, 1}, {1, 2}, {2, 3}});
    const NodePredicates predicates{{"A", [](NodeId) { return true; }}};
    auto counts = [&](const std::string& query) {
        auto result = PGInterpreterTG(query).interpret()->test(chain, {0}, predicates);
        std::set<std::size_t> found;
        for (const auto& match : result->getMatches()) {
            const auto& q = match->getWalks().at(0).quantifiers.at(0);
            REQUIRE(match->getAll().size() == q.count());
            REQUIRE(q.spelling() == query.substr(query.find('{'), query.find('}') - query.find('{') + 1));
            found.insert(q.count());
        }
        return found;
    };
    REQUIRE(counts("A{0}") == std::set<std::size_t>{0});
    REQUIRE(counts("A{2}") == std::set<std::size_t>{2});
    REQUIRE(counts("A{2,3}") == std::set<std::size_t>{2, 3});
    REQUIRE(counts("A{2,}") == std::set<std::size_t>{2, 3, 4});
    REQUIRE(counts("A{5}").empty());
    REQUIRE(counts("A{2,3}->$").empty());
    REQUIRE(counts("A{4}->$") == std::set<std::size_t>{4});

    const auto fork = anchorGraph({"C", "C", "A"}, {{0, 2}, {1, 2}});
    const auto matches = allMatches("(C->A#){2}", fork);
    REQUIRE_FALSE(matches.empty());
    for (const auto& match : matches) {
        REQUIRE(match->getAll() == NodeIds{0, 1, 2});
        REQUIRE(match->getWalks().at(0).quantifiers.at(0).count() == 2);
    }
    REQUIRE(allMatches("(C->A#){3}", fork).empty());
    REQUIRE(allMatches("((C)?->A#){2}", anchorGraph({"A"}, {})).size() == 1);
}

TEST_CASE("Nullable brace repetitions can satisfy required empty iterations", "[app][repetition-range]") {
    const auto empty = anchorGraph({}, {});
    for (const std::string query : {"(A?){2}", "(A?){2,}"}) {
        INFO(query);
        const auto result = PGInterpreterTG(query).interpret()->test(empty, {std::nullopt}, {});
        REQUIRE(result->getMatches().size() == 1);
        const auto& walk = result->getMatches().at(0)->getWalks().at(0);
        REQUIRE(walk.nodes.empty());
        REQUIRE(walk.quantifiers.size() == 3);
        REQUIRE(walk.quantifiers.at(0).iterationEnds == std::vector<std::size_t>{0, 0});
    }
}

TEST_CASE("Brace repetitions agree with a word oracle on directed chains", "[app][repetition-range]") {
    struct Chain {
        std::string word;
        NodeIds ids;
    };
    std::vector<Chain> chains;
    std::vector<std::string> types;
    std::vector<std::pair<NodeId, NodeId>> edges;
    // Enumerate every word over A and B through length five. The endpoint
    // predicates force the matcher to consume the entire word of one chain.
    for (std::size_t length = 0; length <= 5; ++length) {
        for (std::size_t mask = 0; mask < (std::size_t{1} << length); ++mask) {
            Chain chain;
            for (std::size_t bit = 0; bit < length; ++bit)
                chain.word += (mask & (std::size_t{1} << bit)) ? 'B' : 'A';
            const auto first = types.size();
            for (char key : "X" + chain.word + "Y") {
                chain.ids.insert(types.size());
                if (types.size() != first) edges.emplace_back(types.size() - 1, types.size());
                types.emplace_back(1, key);
            }
            chains.push_back(std::move(chain));
        }
    }
    const auto graph = anchorGraph(types, edges);
    struct Atom {
        std::string forward, reverse, wordRegex;
        std::size_t width;
    };
    for (const auto& atom : std::vector<Atom>{{"A", "A", "A", 1},
                                             {"(A|B)", "(A|B)", "(A|B)", 1},
                                             {"(A->B)", "(B<-A)", "(AB)", 2}}) {
        for (const std::string suffix : {"{0}", "{1}", "{2}", "{2,}", "{1,3}", "{0,2}"}) {
            const std::regex oracle(atom.wordRegex + suffix);
            std::map<NodeIds, std::size_t> expected;
            for (const auto& chain : chains)
                if (std::regex_match(chain.word, oracle))
                    expected.emplace(chain.ids, chain.word.size() / atom.width);
            for (const auto& query : {"X->" + atom.forward + suffix + "->Y",
                                      "Y<-" + atom.reverse + suffix + "<-X"}) {
                INFO(query);
                std::map<NodeIds, std::size_t> actual;
                for (const auto& match : allMatches(query, graph)) {
                    const auto& trace = match->getWalks().at(0).quantifiers;
                    REQUIRE(trace.size() == 1);
                    REQUIRE(trace[0].spelling() == suffix);
                    REQUIRE(actual.emplace(match->getAll(), trace[0].count()).second);
                }
                REQUIRE(actual == expected);
            }
        }
    }
}

TEST_CASE("Brace trace allocations retain nested ranges and finite common cycles", "[app][repetition-range]") {
    const auto chain = anchorGraph({"X", "A", "A", "A", "Y"}, {{0, 1}, {1, 2}, {2, 3}, {3, 4}});
    const auto matches = allMatches("X->(A{1,2}){2}->Y", chain);
    REQUIRE(matches.size() == 2);
    std::set<std::pair<std::size_t, std::size_t>> splits;
    for (const auto& match : matches) {
        REQUIRE(match->getAll() == NodeIds{0, 1, 2, 3, 4});
        const auto& q = match->getWalks().at(0).quantifiers;
        REQUIRE(q.size() == 3);
        REQUIRE(q[0].index == 2);
        REQUIRE(q[0].count() == 2);
        REQUIRE(q[1].index == 1);
        REQUIRE(q[2].index == 1);
        REQUIRE(q[1].parent == 0);
        REQUIRE(q[2].parent == 0);
        splits.emplace(q[1].count(), q[2].count());
    }
    REQUIRE(splits == std::set<std::pair<std::size_t, std::size_t>>{{1, 2}, {2, 1}});

    const auto cycle = anchorGraph({"A"}, {{0, 0}});
    auto counts = [&](const std::string& query) {
        std::set<std::size_t> result;
        for (const auto& match : allMatches(query, cycle)) {
            REQUIRE(match->getAll() == NodeIds{0});
            result.insert(match->getWalks().at(0).quantifiers.at(0).count());
        }
        return result;
    };
    REQUIRE(counts("($A$){2,3}") == std::set<std::size_t>{2, 3});
    REQUIRE(counts("($A$){2,}") == std::set<std::size_t>{2});
}

TEST_CASE("Anchored repetitions restart at every distance of a variable prefix", "[regression][app]") {
    // Before, the next iteration restarted at one fixed distance from the anchor:
    // the skip path of A? (distance 0) or a single A for A+.
    const auto fork = anchorGraph({"A", "D", "A"}, {{0, 1}, {2, 1}});
    for (const std::string query : {"(A?->D#)+", "(A*->D#)+", "(A+->D#)+"}) {
        INFO(query);
        bool full = false;
        for (const auto& match : allMatches(query, fork)) full |= match->getAll() == NodeIds{0, 1, 2};
        REQUIRE(full);
    }
    // A0 -> A1 -> D2 <- A4 <- A3: two iterations of two A each.
    const auto chains = anchorGraph({"A", "A", "D", "A", "A"}, {{0, 1}, {1, 2}, {3, 4}, {4, 2}});
    bool counted = false;
    for (const auto& match : allMatches("(A+->D#)+", chains)) {
        if (match->getAll() != NodeIds{0, 1, 2, 3, 4}) continue;
        std::vector<std::pair<std::size_t, std::size_t>> counts;
        for (const auto& q : match->getWalks().at(0).quantifiers) counts.emplace_back(q.index, q.count());
        counted |= counts == std::vector<std::pair<std::size_t, std::size_t>>{{2, 2}, {1, 2}, {1, 2}};
    }
    REQUIRE(counted);
}

TEST_CASE("An anchor identity binds a single source node", "[regression][app]") {
    // Two separate A->D pairs are not one anchored repetition.
    const auto pairs = anchorGraph({"A", "D", "A", "D"}, {{0, 1}, {2, 3}});
    for (const std::string query : {"(A->D#)+", "(A*->D#)+", "(A?->D#)+"})
        for (const auto& match : allMatches(query, pairs)) {
            INFO(query);
            REQUIRE(match->getAll().size() <= 2);
        }
}

TEST_CASE("Repetitions can restart when an optional anchor was skipped", "[regression][app]") {
    const auto fork = anchorGraph({"C", "C", "A"}, {{0, 2}, {1, 2}});
    const auto separate = anchorGraph({"C", "C", "A", "A"}, {{0, 2}, {1, 3}});
    for (const std::string query : {"((D#)?->C?->A#)+", "(A#<-C?<-(D#)?)+", "((D#)?->C*->A#)+"}) {
        INFO(query);
        std::set<NodeIds> captured;
        bool twoIterations = false;
        for (const auto& match : allMatches(query, fork)) {
            captured.insert(match->getAll());
            if (match->getAll() == NodeIds{0, 1, 2})
                twoIterations |= match->getWalks().at(0).quantifiers.at(0).count() == 2;
        }
        REQUIRE(captured == std::set<NodeIds>{{2}, {0, 2}, {1, 2}, {0, 1, 2}});
        REQUIRE(twoIterations);
        for (const auto& match : allMatches(query, separate))
            REQUIRE(match->getAll().size() <= 2);
    }

    const auto chain = anchorGraph({"C", "C"}, {{0, 1}});
    std::set<NodeIds> captured;
    for (const auto& match : allMatches("((D#)?->C)+", chain)) captured.insert(match->getAll());
    REQUIRE(captured == std::set<NodeIds>{{0}, {1}, {0, 1}});
}

TEST_CASE("A return to a captured node does not hide a cycle exit from a boundary", "[boundary][regression][app]") {
    const auto sink = anchorGraph({"A", "B", "C"}, {{0, 1}});
    const auto closed = anchorGraph({"A", "B", "C"}, {{0, 1}, {1, 0}});
    const auto open = anchorGraph({"A", "B", "C"}, {{0, 1}, {1, 0}, {1, 2}});
    const auto branch = anchorGraph({"A", "B", "C"}, {{0, 1}, {1, 2}});
    for (const std::string query : {"A->B#->$", "$<-B#<-A"}) {
        INFO(query);
        for (const auto& graph : {sink, closed}) {
            const auto matches = allMatches(query, graph);
            REQUIRE(matches.size() == 1);
            REQUIRE(matches[0]->getAll() == NodeIds{0, 1});
        }
        REQUIRE(allMatches(query, open).empty());
        REQUIRE(allMatches(query, branch).empty());
    }
    const auto matches = allMatches("A->B#->C->$", open);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0]->getAll() == NodeIds{0, 1, 2});
}

TEST_CASE("An anchor beyond the fixed distance window still restarts a repetition", "[regression][app]") {
    constexpr NodeId length = 33;
    constexpr NodeId anchor = 2 * length;
    std::vector<std::string> types(anchor, "C");
    types.push_back("A");
    std::vector<std::pair<NodeId, NodeId>> edges;
    for (NodeId first : {NodeId{0}, length}) {
        for (NodeId i = first; i + 1 < first + length; ++i) edges.emplace_back(i, i + 1);
        edges.emplace_back(first + length - 1, anchor);
    }
    std::string query = "(";
    for (NodeId i = 0; i < length; ++i) query += "C->";
    query += "A#)+";
    const auto machine = PGInterpreterTG(query).interpret();
    const auto found = machine->test(anchorGraph(types, edges), {0},
        {{"C", [](NodeId id) { return id < anchor; }}, {"A", [](NodeId id) { return id == anchor; }}});
    std::set<std::size_t> sizes;
    for (const auto& match : found->getMatches()) sizes.insert(match->getAll().size());
    REQUIRE(sizes == std::set<std::size_t>{length + 1, 2 * length + 1});
}

TEST_CASE("Paths combine into one solution per set of walks with consistent captures", "[regression][app]") {
    // A0 -> B1 -> C2, searched over every start tuple of the path-order machine.
    const auto chain = anchorGraph({"A", "B", "C"}, {{0, 1}, {1, 2}});
    auto solutions = [&](const std::string& query) {
        auto pattern = PGInterpreterTG(query).interpret();
        NodePredicates predicates;
        for (const std::string key : {"A", "B", "C"})
            predicates[key] = [chain, key](NodeId id) { return chain->getNodes().at(id).predicate == key; };
        std::vector<NodeIds> found;
        const std::size_t arity = pattern->getNbStart();
        std::size_t tuples = 1;
        for (std::size_t i = 0; i < arity; ++i) tuples *= 3;
        for (std::size_t tuple = 0; tuple < tuples; ++tuple) {
            std::vector<std::optional<NodeId>> starts;
            for (std::size_t i = 0, rest = tuple; i < arity; ++i, rest /= 3) starts.push_back(rest % 3);
            const auto result = pattern->test(chain, starts, predicates);
            for (const auto& match : result->getMatches()) found.push_back(match->getAll());
        }
        return found;
    };
    // One solution, not one per order in which the two walks are combined.
    REQUIRE(solutions("A;B") == std::vector<NodeIds>{{0, 1}});
    REQUIRE(solutions("A->B#;B#->C") == std::vector<NodeIds>{{0, 1, 2}});
    // A plain occurrence never shares its node with another occurrence...
    REQUIRE(solutions("A->B;B->C").empty());
    REQUIRE(solutions("A->B;B#->C").empty());
    REQUIRE(solutions("B;B#").empty());
    // ...and one node holds one identity.
    REQUIRE(solutions("B#1;B#2").empty());
}

TEST_CASE("Repetition passes taken in another order are one match", "[regression][app]") {
    // B1..B8 -> D0. From B1, every set of branches containing B1 is one match;
    // before, each set was found once per order of its passes.
    std::vector<std::string> types{"D"};
    std::vector<std::pair<NodeId, NodeId>> edges;
    for (NodeId b = 1; b <= 8; ++b) { types.push_back("B"); edges.push_back({b, 0}); }
    const auto star = anchorGraph(types, edges);
    NodePredicates predicates;
    for (const std::string key : {"B", "D"})
        predicates[key] = [star, key](NodeId id) { return star->getNodes().at(id).predicate == key; };
    const auto matches = PGInterpreterTG("(B->D#)+").interpret()->test(star, {1}, predicates)->getMatches();
    std::set<NodeIds> found;
    for (const auto& match : matches) found.insert(match->getAll());
    REQUIRE(matches.size() == 128);
    REQUIRE(found.size() == 128);
    for (const auto& nodes : found) REQUIRE((nodes.count(0) && nodes.count(1)));

    // Different splits of the same nodes stay separate matches: B0 -> B1 -> B2
    // is B*->B+ with counts 0+3, 1+2 and 2+1.
    const auto chain = anchorGraph({"B", "B", "B"}, {{0, 1}, {1, 2}});
    std::set<std::vector<std::size_t>> counts;
    for (const auto& match : allMatches("B*->B+", chain)) {
        if (match->getAll() != NodeIds{0, 1, 2}) continue;
        std::vector<std::size_t> split;
        for (const auto& q : match->getWalks().at(0).quantifiers) split.push_back(q.count());
        counts.insert(split);
    }
    REQUIRE(counts == std::set<std::vector<std::size_t>>{{0, 3}, {1, 2}, {2, 1}});
}
