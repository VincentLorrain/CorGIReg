#pragma once
// Test-only generation-to-matching harness: generate a graph from a pattern,
// record what was generated, modify it in classified ways, match it and check
// the result with invariants that do not come from the matcher itself.
#include <catch2/catch_test_macros.hpp>
#include "CompileTool/Common/CommonKey.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include "NodeTest/NodeLibrary.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace CorGIReg::GenerationMatching {

using V = NodeValue;
using O = V::Object;
using A = V::Array;

// Fresh operation for added nodes: never a pattern predicate in these tests.
inline const std::string unrelated = "Unrelated";

// Campaign bounds. Defaults are the fast CI mode; CORGIREG_GM_* variables
// override them for extended campaigns (see doc/development.md).
struct Settings {
    std::uint64_t iterations = 40, seed = 20261029, depth = 1, maxLoops = 2, maxNodes = 12,
                  extraNodes = 3, extraEdges = 3, maxStarts = 1000, seconds = 0;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    // Wall-clock budget per test case; 0 is unlimited. Checked between cases.
    bool exhausted() const {
        return seconds && std::chrono::steady_clock::now() - start > std::chrono::seconds(seconds);
    }
    static Settings fromEnvironment(Settings defaults) {
        auto read = [](const char* name, std::uint64_t& value) {
            if (const char* text = std::getenv(name); text && *text) value = std::stoull(text);
        };
        read("CORGIREG_GM_ITERATIONS", defaults.iterations);
        read("CORGIREG_GM_SEED", defaults.seed);
        read("CORGIREG_GM_DEPTH", defaults.depth);
        read("CORGIREG_GM_MAX_LOOPS", defaults.maxLoops);
        read("CORGIREG_GM_MAX_NODES", defaults.maxNodes);
        read("CORGIREG_GM_EXTRA_NODES", defaults.extraNodes);
        read("CORGIREG_GM_EXTRA_EDGES", defaults.extraEdges);
        read("CORGIREG_GM_MAX_STARTS", defaults.maxStarts);
        read("CORGIREG_GM_SECONDS", defaults.seconds);
        defaults.start = std::chrono::steady_clock::now();
        return defaults;
    }
};

struct Node {
    std::string op;          // Source operation (a library op_type in library mode).
    std::string predicate;   // Pattern predicate that generated it; empty for added nodes.
    std::string identity;    // Generator key: plain key or common identity.
    bool common = false;
};
struct Edge { NodeId from, to; EdgePorts ports; };

// Editable operation graph. Generated node IDs follow GraphNN insertion order.
struct Graph {
    std::map<NodeId, Node> nodes;
    std::vector<Edge> edges;

    NodeId add(Node node) {
        const NodeId id = nodes.empty() ? 0 : nodes.rbegin()->first + 1;
        nodes.emplace(id, std::move(node));
        return id;
    }
    void connect(NodeId from, NodeId to, EdgePorts ports = {}) { edges.push_back({from, to, ports}); }
    NodeIds ids() const { NodeIds result; for (const auto& entry : nodes) result.insert(entry.first); return result; }
    NodeIds parents(NodeId id) const { NodeIds r; for (const auto& e : edges) if (e.to == id) r.insert(e.from); return r; }
    NodeIds children(NodeId id) const { NodeIds r; for (const auto& e : edges) if (e.from == id) r.insert(e.to); return r; }

    std::shared_ptr<ImportedGraph> view() const {
        auto graph = std::make_shared<ImportedGraph>();
        for (const auto& [id, node] : nodes) graph->addNode(id, node.op, node.op + "_" + std::to_string(id));
        for (const auto& edge : edges) graph->addEdge(edge.from, edge.to, edge.ports);
        return graph;
    }
    std::map<NodeId, NodeValue> documents() const {
        std::map<NodeId, NodeValue> result;
        for (const auto& [id, node] : nodes) result.emplace(id, O{{"op", node.op}});
        return result;
    }
    // Graph JSON accepted by corgireg; extra node fields record generation metadata.
    NodeValue json() const {
        A jsonNodes, jsonEdges, connections;
        for (const auto& [id, node] : nodes)
            jsonNodes.push_back(O{{"id", V(std::uint64_t(id))}, {"op", node.op}, {"predicate_key", node.predicate},
                                  {"identity", node.identity}, {"common", node.common}});
        std::set<std::pair<NodeId, NodeId>> pairs;
        for (const auto& edge : edges) {
            pairs.emplace(edge.from, edge.to);
            auto port = [](const std::optional<PortNumber>& value) { return value ? V(std::uint64_t(*value)) : V(nullptr); };
            connections.push_back(O{{"from", V(std::uint64_t(edge.from))}, {"to", V(std::uint64_t(edge.to))},
                                    {"input", port(edge.ports.input)}, {"output", port(edge.ports.output)}});
        }
        for (const auto& [from, to] : pairs) jsonEdges.push_back(A{V(std::uint64_t(from)), V(std::uint64_t(to))});
        return O{{"nodes", jsonNodes}, {"edges", jsonEdges}, {"connections", connections}};
    }
};

// Syntactic facts read from the AST. Quantifier indices follow the lexical
// order of their symbols, as in IRGenerationOptions and match traces.
struct PatternInfo {
    struct Quantifier { char symbol; bool common; std::set<std::size_t> nested, enclosing; };
    std::set<std::string> predicates;
    std::map<std::string, std::set<std::string>> identities;      // predicate -> identities used
    std::map<std::string, unsigned> plainOccurrences;              // predicate -> plain key count
    std::map<std::string, std::pair<bool, bool>> strict;           // identity -> (inputs, outputs)
    std::vector<Quantifier> quantifiers;
    bool boundary = false;

    explicit PatternInfo(const std::string& query) {
        using Tree = std::shared_ptr<AstNode<tGTokensTypes>>;
        std::vector<std::size_t> open;
        std::function<bool(const Tree&)> visit = [&](const Tree& tree) {
            const auto type = tree->getType();
            const bool quantifier = type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO;
            const auto first = quantifiers.size();
            bool common = false;
            for (const auto& child : tree->getChilds()) common |= visit(child);
            if (type == tGTokensTypes::END) boundary = true;
            if (type == tGTokensTypes::KEY) {
                predicates.insert(tree->getValue());
                identities[tree->getValue()].insert(tree->getValue());
                ++plainOccurrences[tree->getValue()];
            }
            if (type == tGTokensTypes::CKEY || type == tGTokensTypes::SCKEY) {
                const auto key = parseCommonKey(tree->getValue());
                predicates.insert(key.predicate);
                identities[key.predicate].insert(key.identity());
                auto& flags = strict[key.identity()];
                flags.first |= key.strictInputs;
                flags.second |= key.strictOutputs;
                common = true;
            }
            if (quantifier) {
                Quantifier info{type == tGTokensTypes::QOM ? '+' : type == tGTokensTypes::QZM ? '*' : '?', common, {}, {}};
                for (auto i = first; i < quantifiers.size(); ++i) {
                    info.nested.insert(i);
                    quantifiers[i].enclosing.insert(quantifiers.size());
                }
                quantifiers.push_back(info);
            }
            return common;
        };
        visit(PartitionGraphParser(query).parse());
    }
    // Known matcher limitation, excluded from generator oracles: a quantifier
    // nested in a repeated group containing a common anchor, e.g. (A?->D#)+
    // or (A+->D#)+, misses whole-graph allocations on explicit graphs too.
    bool nestedInAnchoredRepetition() const {
        for (const auto& q : quantifiers) if (q.common && !q.nested.empty()) return true;
        return false;
    }
    // Generated nodes are the only nodes with their operation and every
    // element is mandatory: any match must cover exactly the generated nodes.
    bool distinctMandatory() const {
        if (!quantifiers.empty() || boundary) return false;
        for (const auto& [predicate, used] : identities)
            if (used.size() != 1 || plainOccurrences.count(predicate) && plainOccurrences.at(predicate) > 1) return false;
        return true;
    }
};

struct Case {
    std::uint64_t seed = 0;
    std::string query;
    std::map<std::size_t, std::size_t> repetitions;     // Quantifier index -> count of every invocation.
    std::vector<std::string> libraryPaths;
    std::string format = "json";
    Graph generated, graph;
    std::vector<std::string> mutations;
    std::uint64_t maxStarts = 1000;
    // Lazily parsed from query and libraryPaths; copies share them.
    mutable std::shared_ptr<PatternInfo> info;
    mutable std::shared_ptr<NodeLibrary> loaded;

    const PatternInfo& pattern() const {
        if (!info) info = std::make_shared<PatternInfo>(query);
        return *info;
    }
    const NodeLibrary& library() const {
        if (!loaded) {
            std::vector<std::filesystem::path> paths(libraryPaths.begin(), libraryPaths.end());
            loaded = std::make_shared<NodeLibrary>(NodeLibrary::load(paths));
        }
        return *loaded;
    }
    std::pair<bool, bool> strict(const Node& node) const {
        auto found = pattern().strict.find(node.identity);
        return node.common && found != pattern().strict.end() ? found->second : std::pair<bool, bool>{};
    }
    // Documented semantics: a plain key has at most one parent and one child;
    // $-marked common sides require every neighbor on that side to be covered.
    bool acceptsParent(NodeId id) const {
        const auto& node = graph.nodes.at(id);
        return node.common ? !strict(node).first : graph.parents(id).empty();
    }
    bool acceptsChild(NodeId id) const {
        const auto& node = graph.nodes.at(id);
        return node.common ? !strict(node).second : graph.children(id).empty();
    }

    // Same graph and mutations, matched with another pattern.
    Case withQuery(std::string other) const {
        Case result = *this;
        result.query = std::move(other);
        result.info.reset();
        return result;
    }

    static Case generate(std::uint64_t seed, std::string query, const Settings& settings,
                         std::vector<std::string> libraryPaths = {}, std::string format = "json") {
        Case result;
        result.seed = seed;
        result.query = std::move(query);
        result.libraryPaths = std::move(libraryPaths);
        result.format = std::move(format);
        result.maxStarts = settings.maxStarts;
        std::mt19937 random(static_cast<std::uint32_t>(seed));
        IRGenerationOptions options;
        options.maxNodes = settings.maxNodes;
        for (std::size_t i = 0; i < result.pattern().quantifiers.size(); ++i) {
            const auto symbol = result.pattern().quantifiers[i].symbol;
            const std::size_t low = symbol == '+', high = symbol == '?' ? 1 : settings.maxLoops + low;
            result.repetitions[i + 1] = std::uniform_int_distribution<std::size_t>(low, std::max(low, high))(random);
            options.repetitions[i + 1] = {result.repetitions[i + 1]};
        }
        const auto generated = PGInterpreterIR(result.query, options).interpret();
        for (const auto& node : generated->getNodes()) {
            const auto predicate = node->getType();
            auto op = predicate;
            if (!result.libraryPaths.empty())
                if (auto concrete = result.library().generatedOperation(predicate, result.format)) op = *concrete;
            result.generated.add({op, predicate, node->getInfo<std::string>("Key", predicate), node->hasArgument("isCommon")});
        }
        BasicGraphAdapter adapter(*generated);
        for (const auto& node : generated->getNodes())
            for (auto child : adapter.children(adapter.id(node)))
                for (const auto& ports : adapter.edgePorts(adapter.id(node), child))
                    result.generated.connect(adapter.id(node), child, ports);
        result.graph = result.generated;
        return result;
    }
};

// Match-preserving modifications. Added nodes never satisfy a pattern
// predicate except disconnected decoys, and every step is recorded.
inline NodeId addUnrelated(Case& item) { return item.graph.add({unrelated, "", "", false}); }

template<class Accept> std::vector<NodeId> accepting(const Case& item, Accept accept) {
    std::vector<NodeId> result;
    for (const auto& [id, node] : item.generated.nodes) if (accept(id)) result.push_back(id);
    return result;
}

// Decoy component reusing generated operations, with random edges, cycles and self-loops.
inline void addDisconnected(Case& item, std::mt19937& random, std::size_t nodes, std::size_t edges) {
    if (!nodes) return;
    std::vector<std::string> ops{unrelated};
    for (const auto& [id, node] : item.generated.nodes) ops.push_back(node.op);
    std::vector<NodeId> added;
    for (std::size_t i = 0; i < nodes; ++i)
        added.push_back(item.graph.add({ops[random() % ops.size()], "", "", false}));
    for (std::size_t i = 0; i < edges; ++i)
        item.graph.connect(added[random() % added.size()], added[random() % added.size()]);
    item.mutations.push_back("disconnected component: " + std::to_string(nodes) + " nodes, " + std::to_string(edges) + " edges");
}

// Unrelated producers before and consumers after the generated subgraph.
inline void embed(Case& item, std::mt19937& random) {
    auto sources = accepting(item, [&](NodeId id) { return item.acceptsParent(id); });
    if (!sources.empty()) {
        const auto target = sources[random() % sources.size()];
        const auto first = addUnrelated(item), second = addUnrelated(item);
        item.graph.connect(first, second);
        item.graph.connect(second, target);
        item.mutations.push_back("upstream chain into " + std::to_string(target));
    }
    auto sinks = accepting(item, [&](NodeId id) { return item.acceptsChild(id); });
    if (!sinks.empty()) {
        const auto origin = sinks[random() % sinks.size()];
        const auto first = addUnrelated(item), second = addUnrelated(item);
        item.graph.connect(origin, first);
        item.graph.connect(first, second);
        item.mutations.push_back("downstream chain from " + std::to_string(origin));
    }
}

// Extra unrelated branches, with a cycle outside the generated subgraph.
inline void addBranches(Case& item, std::mt19937& random, std::size_t count) {
    for (std::size_t i = 0; i < count && !item.generated.nodes.empty(); ++i) {
        const NodeId id = random() % item.generated.nodes.size();
        if (item.acceptsParent(id)) {
            const auto cycle = addUnrelated(item), producer = addUnrelated(item);
            item.graph.connect(cycle, producer);
            item.graph.connect(producer, cycle);
            item.graph.connect(producer, id);
            item.mutations.push_back("cyclic producer into " + std::to_string(id));
        } else if (item.acceptsChild(id)) {
            const auto consumer = addUnrelated(item);
            item.graph.connect(id, consumer);
            item.graph.connect(consumer, consumer);
            item.mutations.push_back("self-looping consumer from " + std::to_string(id));
        }
    }
}

// Extra edges between generated anchors whose touched sides are not strict.
inline void linkAnchors(Case& item, std::mt19937& random, std::size_t count) {
    auto from = accepting(item, [&](NodeId id) { return item.graph.nodes.at(id).common && item.acceptsChild(id); });
    auto to = accepting(item, [&](NodeId id) { return item.graph.nodes.at(id).common && item.acceptsParent(id); });
    for (std::size_t i = 0; i < count && !from.empty() && !to.empty(); ++i) {
        const auto source = from[random() % from.size()], target = to[random() % to.size()];
        item.graph.connect(source, target);
        item.mutations.push_back("anchor edge " + std::to_string(source) + "->" + std::to_string(target));
    }
}

// Constraint-violating mutations. Callers assert "no-match" only when the
// mutation invalidates every interpretation (see PatternInfo::distinctMandatory).
inline void removeEdge(Case& item, NodeId from, NodeId to) {
    auto& edges = item.graph.edges;
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                               [&](const Edge& edge) { return edge.from == from && edge.to == to; }), edges.end());
    item.mutations.push_back("remove edge " + std::to_string(from) + "->" + std::to_string(to));
}
inline void retype(Case& item, NodeId id) {
    item.graph.nodes.at(id).op = unrelated;
    item.mutations.push_back("retype " + std::to_string(id) + " to " + unrelated);
}
inline void addParent(Case& item, NodeId id) {
    item.graph.connect(addUnrelated(item), id);
    item.mutations.push_back("extra parent of " + std::to_string(id));
}
inline void addChild(Case& item, NodeId id) {
    item.graph.connect(id, addUnrelated(item));
    item.mutations.push_back("extra child of " + std::to_string(id));
}
inline void setInputPort(Case& item, NodeId from, NodeId to, std::optional<PortNumber> port) {
    for (auto& edge : item.graph.edges) if (edge.from == from && edge.to == to) edge.ports.input = port;
    item.mutations.push_back("input port of " + std::to_string(from) + "->" + std::to_string(to) + " = " +
                             (port ? std::to_string(*port) : std::string("unspecified")));
}
// Moves the edges from `id` to `children` onto a copy of `id`.
inline NodeId split(Case& item, NodeId id, const NodeIds& children) {
    auto copy = item.graph.nodes.at(id);
    const auto twin = item.graph.add(copy);
    for (auto& edge : item.graph.edges) if (edge.from == id && children.count(edge.to)) edge.from = twin;
    item.mutations.push_back("split " + std::to_string(id) + " into " + std::to_string(twin));
    return twin;
}

struct Search {
    std::vector<std::shared_ptr<MatchSolution>> matches;
    bool truncated = false;
};

// Enumerates starting tuples like the CLI. generatedStarts limits candidates to
// generated nodes: every match covering only generated nodes starts there.
inline Search search(const Case& item, bool generatedStarts = false) {
    const auto graph = item.graph.view();
    NodePredicates predicates;
    for (const auto& [id, node] : item.graph.nodes) {
        const auto op = node.op;
        predicates[op] = [graph, op](NodeId candidate) { return graph->getNodes().at(candidate).predicate == op; };
    }
    if (!item.libraryPaths.empty())
        addDefinitionPredicates(predicates, item.library().resolve(item.format, item.graph.documents()));
    std::vector<std::optional<NodeId>> candidates;
    for (const auto& [id, node] : item.graph.nodes)
        if (!generatedStarts || item.generated.nodes.count(id)) candidates.push_back(id);
    if (candidates.empty()) candidates.push_back(std::nullopt);
    const auto pattern = PGInterpreterTG(item.query).interpret();
    std::uint64_t combinations = 1;
    for (std::size_t i = 0; i < pattern->getNbStart(); ++i) {
        if (combinations > item.maxStarts / candidates.size()) return {{}, true};
        combinations *= candidates.size();
    }
    Search result;
    for (std::uint64_t combination = 0; combination < combinations; ++combination) {
        auto rest = combination;
        std::vector<std::optional<NodeId>> starts;
        for (std::size_t i = 0; i < pattern->getNbStart(); ++i) {
            starts.push_back(candidates[rest % candidates.size()]);
            rest /= candidates.size();
        }
        const auto found = pattern->test(graph, starts, predicates);
        result.matches.insert(result.matches.end(), found->getMatches().begin(), found->getMatches().end());
    }
    return result;
}

// Node set and quantifier counts of every match, for order-independent comparison.
inline std::set<std::pair<NodeIds, std::vector<std::size_t>>> signatures(const Search& result) {
    std::set<std::pair<NodeIds, std::vector<std::size_t>>> out;
    for (const auto& match : result.matches) {
        std::vector<std::size_t> counts;
        for (const auto& walk : match->getWalks())
            for (const auto& q : walk.quantifiers) { counts.push_back(q.index); counts.push_back(q.count()); }
        out.emplace(match->getAll(), counts);
    }
    return out;
}

// Checks every match against the graph and the pattern text, not the matcher.
inline std::string invariantError(const Case& item, const Search& result) {
    const auto& pattern = item.pattern();
    for (const auto& match : result.matches) {
        NodeIds captured;
        for (const auto& walk : match->getWalks()) {
            for (const auto& capture : walk.nodes) {
                captured.insert(capture.node);
                auto node = item.graph.nodes.find(capture.node);
                if (node == item.graph.nodes.end()) return "capture of unknown node " + std::to_string(capture.node);
                if (!pattern.predicates.count(capture.predicate))
                    return "capture predicate '" + capture.predicate + "' is not in the pattern";
                const auto* definition = item.libraryPaths.empty() ? nullptr : item.library().find(capture.predicate);
                const auto accepted = definition ? definition->operations(item.format) : std::vector<std::string>{capture.predicate};
                if (std::find(accepted.begin(), accepted.end(), node->second.op) == accepted.end())
                    return "node " + std::to_string(capture.node) + " (" + node->second.op +
                           ") captured as '" + capture.predicate + "'";
            }
            for (const auto& q : walk.quantifiers) {
                if (q.index == 0 || q.index > pattern.quantifiers.size()) return "unknown quantifier index";
                if (q.symbol != pattern.quantifiers[q.index - 1].symbol) return "quantifier symbol mismatch";
                if ((q.symbol == '+' && q.count() == 0) || (q.symbol == '?' && q.count() > 1))
                    return "quantifier " + std::to_string(q.index) + " count " + std::to_string(q.count());
            }
        }
        if (captured != match->getAll()) return "node_ids differ from the union of walk captures";
    }
    return {};
}

// Counts are only predictable for quantifiers whose bodies, and enclosing
// bodies, contain no common anchor and cannot become empty.
inline bool countsExpected(const Case& item, std::size_t index) {
    const auto& quantifiers = item.pattern().quantifiers;
    const auto& q = quantifiers[index - 1];
    if (q.common) return false;
    for (auto outer : q.enclosing) if (quantifiers[outer].common) return false;
    for (auto inner : q.nested) if (item.repetitions.at(inner + 1) == 0) return false;
    return true;
}

// The generated subgraph is found with the generated quantifier counts.
inline bool hasExpectedMatch(const Case& item, const Search& result) {
    const auto expected = item.generated.ids();
    for (const auto& match : result.matches) {
        if (match->getAll() != expected) continue;
        bool counts = true;
        for (const auto& walk : match->getWalks())
            for (const auto& q : walk.quantifiers)
                if (countsExpected(item, q.index) && q.count() != item.repetitions.at(q.index)) counts = false;
        if (counts) return true;
    }
    return false;
}

inline NodeValue matchesJson(const Search& result) {
    A matches;
    for (const auto& match : result.matches) {
        A ids, counts;
        for (auto id : match->getAll()) ids.push_back(V(std::uint64_t(id)));
        for (const auto& walk : match->getWalks())
            for (const auto& q : walk.quantifiers)
                counts.push_back(O{{"index", V(std::uint64_t(q.index))}, {"count", V(std::uint64_t(q.count()))}});
        matches.push_back(O{{"node_ids", ids}, {"quantifiers", counts}});
    }
    return matches;
}

// Everything needed to replay one case without rerunning its campaign.
inline NodeValue caseJson(const Case& item, const std::string& expectation, const Search& actual) {
    A paths, mutations, expected;
    for (const auto& path : item.libraryPaths) paths.push_back(std::filesystem::absolute(path).string());
    for (const auto& mutation : item.mutations) mutations.push_back(mutation);
    for (auto id : item.generated.ids()) expected.push_back(V(std::uint64_t(id)));
    O repetitions;
    for (const auto& [index, count] : item.repetitions) repetitions.emplace(std::to_string(index), V(std::uint64_t(count)));
    return O{{"seed", V(item.seed)}, {"query", item.query}, {"repetitions", repetitions},
             {"node_library", paths}, {"format", item.format}, {"max_starts", V(item.maxStarts)},
             {"generated_graph", item.generated.json()}, {"graph", item.graph.json()}, {"mutations", mutations},
             {"expectation", expectation}, {"expected_node_ids", expected}, {"actual_matches", matchesJson(actual)}};
}

// Directory for failure artifacts: CORGIREG_GM_ARTIFACTS, else the test's
// working directory (build/test under CTest).
inline std::string saveFailure(const Case& item, const std::string& expectation, const Search& actual) {
    const char* configured = std::getenv("CORGIREG_GM_ARTIFACTS");
    const std::filesystem::path folder = configured && *configured ? configured : "generation-matching-failures";
    std::filesystem::create_directories(folder);
    const auto path = folder / ("seed-" + std::to_string(item.seed) + "-" + std::to_string(item.mutations.size()) +
                                "-" + expectation + ".json");
    std::ofstream(path, std::ios::binary) << [&] { std::ostringstream out; writeJson(out, caseJson(item, expectation, actual)); return out.str(); }() << '\n';
    return std::filesystem::absolute(path).string();
}

inline std::string describe(const Case& item) {
    std::ostringstream out;
    out << "seed=" << item.seed << "\nquery=" << item.query << "\nrepetitions=";
    for (const auto& [index, count] : item.repetitions) out << index << ':' << count << ' ';
    out << "\nformat=" << item.format << " node_library=";
    for (const auto& path : item.libraryPaths) out << path << ' ';
    out << "\nmutations=";
    for (const auto& mutation : item.mutations) out << '[' << mutation << "] ";
    out << "\ngenerated=";
    writeJson(out, item.generated.json());
    out << "\ngraph=";
    writeJson(out, item.graph.json());
    return out.str();
}

// expectation: "match" (generated subgraph found with its counts) or
// "no-match" (no match at all). Failures save a replayable artifact.
// Returns false, without asserting, when the start budget is exceeded.
inline bool check(const Case& item, const std::string& expectation) {
    INFO(describe(item));
    Search result;
    try { result = search(item, expectation == "match"); }
    catch (const std::exception& error) {
        INFO("artifact=" << saveFailure(item, expectation, {}));
        FAIL("matching threw: " << error.what());
    }
    if (result.truncated) return false;
    const auto invariant = invariantError(item, result);
    const bool ok = invariant.empty() &&
        (expectation == "match" ? hasExpectedMatch(item, result) : result.matches.empty());
    if (!ok) {
        std::ostringstream actual;
        writeJson(actual, matchesJson(result));
        INFO("invariant=" << invariant << "\nexpected=" << expectation << "\nactual=" << actual.str()
             << "\nartifact=" << saveFailure(item, expectation, result));
        REQUIRE(ok);
    }
    return true;
}

}
