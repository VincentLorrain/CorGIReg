#include "Cli/MatchRequest.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "GraphAdapters/BasicGraphAdapter.hpp"
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"
#include "GraphAdapters/ImportedGraph.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <set>
#include <stdexcept>

using namespace CorGIReg;
namespace {
struct Case {
    std::string name;
    std::size_t nodes, starts, expected;
    std::function<std::size_t()> operation;
};

Case chain(std::size_t size) {
    IRGenerationOptions options;
    options.repetitions = {{1, {size}}};
    auto source = PGInterpreterIR("A+", options).interpret();
    auto graph = std::make_shared<BasicGraphAdapter>(*source);
    if (source->getNodes().size() != size) throw std::runtime_error("Incorrect generated chain size");
    for (NodeId id = 0; id < size; ++id)
        if (graph->children(id) != (id + 1 < size ? NodeIds{id + 1} : NodeIds{}) ||
            graph->parents(id) != (id ? NodeIds{id - 1} : NodeIds{}))
            throw std::runtime_error("Incorrect generated chain topology");
    auto pattern = PGInterpreterTG("A+->$").interpret();
    NodePredicates predicates{{"A", [](NodeId) { return true; }}};
    return {"match_chain_" + std::to_string(size), size, 1, 1, [graph, pattern, predicates, size] {
        auto result = pattern->test(graph, {0}, predicates);
        for (const auto& match : result->getMatches()) {
            if (match->getAll().size() != size || match->getWalks().size() != 1 ||
                match->getWalks()[0].quantifiers.at(0).count() != size)
                throw std::runtime_error("Incorrect chain capture or repetition count");
        }
        return result->getMatches().size();
    }};
}

Case generation(std::size_t size) {
    IRGenerationOptions options;
    options.repetitions = {{1, {size}}};
    return {"generate_chain_" + std::to_string(size), size, 0, size, [options, size] {
        auto graph = PGInterpreterIR("A+", options).interpret();
        std::size_t edges = 0;
        for (const auto& node : graph->getNodes()) edges += node->getChildren().size();
        if (edges != size - 1) throw std::runtime_error("Incorrect generated edge count");
        // GraphNN uses strong reciprocal links. Release this measured temporary
        // fixture explicitly instead of accumulating it over benchmark samples.
        for (const auto& node : graph->getNodes()) {
            auto children = node->getChildren();
            for (const auto& child : children) { child->removeParent(node); node->removeChild(child); }
        }
        return graph->getNodes().size();
    }};
}

std::string hex(const std::string& text) {
    const char* digits = "0123456789abcdef";
    std::string result;
    for (unsigned char c : text) { result += digits[c >> 4]; result += digits[c & 15]; }
    return result;
}

Case search(std::size_t triples, std::size_t jobs = 1) {
    std::ostringstream request;
    request << "CORGiReg1 " << triples * 3 << ' ';
    for (std::size_t id = 0; id < triples * 3; ++id)
        request << id << ' ' << hex(std::string(1, "ABC"[id % 3])) << " 6e ";
    request << triples * 2 << ' ';
    for (std::size_t i = 0; i < triples; ++i)
        request << i * 3 << ' ' << i * 3 + 1 << ' ' << i * 3 + 1 << ' ' << i * 3 + 2 << ' ';
    request << hex("A->B#;B#->C") << " 100000";
    const auto name = "search_shared_" + std::to_string(triples * 3) +
                      (jobs == 1 ? "" : "_jobs" + std::to_string(jobs));
    return {name, triples * 3, 2, triples, [text = request.str(), jobs] {
        std::istringstream in(text);
        std::ostringstream out;
        matchRequest(in, out, jobs);
        const auto result = out.str();
        // Several execution traces can describe the same source-node grouping.
        std::set<std::string> groups;
        std::size_t position = 0;
        while ((position = result.find("\"node_ids\":", position)) != std::string::npos) {
            const auto begin = result.find('[', position);
            const auto end = result.find(']', begin);
            if (begin == std::string::npos || end == std::string::npos)
                throw std::runtime_error("Malformed matching report");
            groups.insert(result.substr(begin, end - begin + 1));
            position = end + 1;
        }
        return groups.size();
    }};
}

unsigned positive(const std::string& value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("Expected positive integer");
    auto n = std::stoul(value);
    if (!n || n > 1000) throw std::invalid_argument("Sampling options must be between 1 and 1000");
    return static_cast<unsigned>(n);
}
}

int main(int argc, char** argv) {
    try {
        unsigned samples = 7, iterations = 3;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 == argc) throw std::invalid_argument("Missing option value");
            std::string option = argv[i];
            if (option == "--samples") samples = positive(argv[i + 1]);
            else if (option == "--iterations") iterations = positive(argv[i + 1]);
            else throw std::invalid_argument("Unknown option: " + option);
        }
        const std::string query = "A+->B#;B#->C#;C#->D";
        auto ast = PartitionGraphParser(query).parse();
        std::vector<Case> cases{
            {"compile_shared", 0, 3, 1, [query] { return PGInterpreterTG(query).interpret()->getTransitions().empty() ? 0 : 1; }},
            {"post_ast_shared", 0, 3, 1, [ast] {
                auto plan = RootSelectionPass{}.run(ast);
                if (plan.components.size() != 1 || !plan.components[0].root ||
                    plan.nodes[*plan.components[0].root].occurrences.front()->getValue() != "B#")
                    throw std::runtime_error("Incorrect root selection");
                return plan.components.size();
            }},
            generation(64), chain(16), chain(64), search(2), search(4), search(16),
            search(16, 2), search(16, 4), search(64), search(64, 2), search(64, 4)
        };
        std::cout << std::setprecision(17) << "{\"schema_version\":1,\"compiler\":\"" << CORGIREG_BENCH_COMPILER
                  << "\",\"configuration\":\"" << CORGIREG_BENCH_CONFIG
                  << "\",\"warmups\":3,\"iterations\":" << iterations << ",\"cases\":[";
        bool first = true;
        for (const auto& benchmark : cases) {
            for (unsigned i = 0; i < 3; ++i)
                if (benchmark.operation() != benchmark.expected) throw std::runtime_error("Incorrect warmup result: " + benchmark.name);
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"name\":\"" << benchmark.name << "\",\"graph_nodes\":" << benchmark.nodes
                      << ",\"start_arity\":" << benchmark.starts << ",\"expected_result\":" << benchmark.expected
                      << ",\"samples_ns\":[";
            for (unsigned i = 0; i < samples; ++i) {
                std::size_t results = 0;
                const auto begin = std::chrono::steady_clock::now();
                for (unsigned j = 0; j < iterations; ++j) results += benchmark.operation();
                const auto duration = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin).count();
                if (results != benchmark.expected * iterations) throw std::runtime_error("Incorrect measured result: " + benchmark.name);
                if (i) std::cout << ',';
                std::cout << duration / iterations;
            }
            std::cout << "]}";
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark failed: " << error.what() << '\n';
        return 1;
    }
}
