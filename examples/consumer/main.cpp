// Consumer-side demonstration only: no rewrite engine is added to CorGIReg.
#include <CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp>
#include <CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp>
#include <GraphAdapters/BasicGraphAdapter.hpp>
#include <GraphAdapters/ImportedGraph.hpp>
#include <GraphAdapters/GraphJson.hpp>
#include <cmath>
#include <iostream>
#include <sstream>

using namespace CorGIReg;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// One explicit recipe: scalar Sub(x,y) -> Add(x,Neg(y)). The host owns graph
// mutation and the input/output mapping, not the matching library.
ImportedGraph replaceSub(const ImportedGraph& source, NodeId matched, const GraphNN& replacement) {
    BasicGraphAdapter fragment(replacement);
    check(replacement.getNodes().size() == 2 && fragment.node(0)->getType() == "Neg" &&
          fragment.node(1)->getType() == "Add", "Unexpected replacement operators");
    ImportedGraph output;
    for (const auto& item : source.getNodes())
        if (item.first != matched) output.addNode(item.first, item.second.predicate, item.second.name);
    // Allocate IDs beyond the original namespace, preserving all surviving IDs.
    const NodeId first = source.getNodes().rbegin()->first + 1;
    output.addNode(first, "Neg", "replacement/neg");
    output.addNode(first + 1, "Add", "replacement/add");
    for (NodeId i = 0; i < 2; ++i) for (auto child : fragment.children(i))
        for (auto ports : fragment.edgePorts(i, child)) {
            ports.output = 1;
            output.addEdge(first + i, first + child, ports);
        }
    std::set<PortNumber> inputs;
    for (const auto& item : source.getNodes()) for (auto child : item.second.children)
        for (auto ports : source.edgePorts(item.first, child)) {
            if (child == matched) {
                check(item.first != matched && ports.input && (*ports.input == 1 || *ports.input == 2), "Invalid Sub input");
                check(inputs.insert(*ports.input).second, "Sub input has multiple producers");
                const auto destination = *ports.input == 1 ? first + 1 : first;
                ports.input = 1;
                output.addEdge(item.first, destination, ports);
            } else if (item.first == matched) {
                check(ports.output == 1, "Unsupported Sub output");
                output.addEdge(first + 1, child, ports);
            } else output.addEdge(item.first, child, ports);
        }
    check(inputs == std::set<PortNumber>{1, 2}, "Missing Sub input");
    return output;
}

double evaluate(const ImportedGraph& graph, NodeId node, double x, double y) {
    const auto& operation = graph.getNodes().at(node).predicate;
    if (operation == "X") return x;
    if (operation == "Y") return y;
    std::map<PortNumber, double> arguments;
    for (auto parent : graph.parents(node)) for (auto ports : graph.edgePorts(parent, node)) {
        check(ports.input.has_value(), "Unnumbered evaluator input");
        check(arguments.emplace(*ports.input, evaluate(graph, parent, x, y)).second, "Duplicate evaluator input");
    }
    if (operation == "Sub") return arguments.at(1) - arguments.at(2);
    if (operation == "Neg") return -arguments.at(1);
    if (operation == "Add") return arguments.at(1) + arguments.at(2);
    return arguments.begin()->second; // Outside identity consumers.
}
}

int main() {
    try {
        auto source = std::make_shared<ImportedGraph>();
        for (const auto& item : std::map<NodeId, std::string>{{10,"X"},{20,"Y"},{30,"Sub"},{40,"Out"},{50,"Side"},{60,"Out"}})
            source->addNode(item.first, item.second, "original/" + std::to_string(item.first));
        source->addEdge(10, 30, {1, 1}); source->addEdge(20, 30, {2, 2});
        source->addEdge(30, 40, {1, 1}); source->addEdge(30, 60, {3, 1});
        source->addEdge(10, 50, {1, 1});
        auto pattern = PGInterpreterTG("Sub#").interpret();
        NodePredicates predicates{{"Sub", [source](NodeId id) { return source->getNodes().at(id).predicate == "Sub"; }}};
        auto matches = pattern->test(source, {30}, predicates);
        check(!matches->getMatches().empty(), "Sub not found");
        // Several outgoing walks can describe the same captured subgraph.
        for (const auto& match : matches->getMatches())
            check(match->getAll() == NodeIds{30}, "Unexpected matched subgraph");
        check(pattern->test(source, {10}, predicates)->getMatches().empty(), "False positive Sub match");
        const auto captured = matches->getMatches().front()->getAll();
        check(captured == NodeIds{30}, "Incorrect source identity");
        auto replacement = PGInterpreterIR("Neg-2->Add#", IRGenerationOptions{}).interpret();
        std::ostringstream json; writeGraphJson(*replacement, json);
        check(json.str().find("\"input\":2") != std::string::npos, "Export lost port");
        auto result = replaceSub(*source, *captured.begin(), *replacement);
        check(result.getNodes().count(30) == 0 && source->getNodes().count(30) == 1, "Source mutated or Sub retained");
        check(result.edgePorts(10, 62) == std::set<EdgePorts>{{1,1}}, "Left input lost");
        check(result.edgePorts(20, 61) == std::set<EdgePorts>{{1,2}}, "Right input source output lost");
        check(result.edgePorts(61, 62) == std::set<EdgePorts>{{2,1}}, "Internal binding lost");
        check(result.edgePorts(62, 40) == std::set<EdgePorts>{{1,1}} &&
              result.edgePorts(62, 60) == std::set<EdgePorts>{{3,1}}, "Output fanout lost");
        check(result.edgePorts(10, 50) == source->edgePorts(10, 50), "Outside branch changed");
        check(result.nodeName(50) == source->nodeName(50), "Outside identity lost");
        for (double x : {-8., 0., 3., 10.}) for (double y : {-5., 0., 2., 7.}) {
            for (NodeId output : {40u, 50u, 60u})
                check(evaluate(*source, output, x, y) == evaluate(result, output, x, y), "Recipe changed result");
        }
        auto invalid = *source;
        invalid.addEdge(10, 30, {2, 1});
        bool rejected = false;
        try { replaceSub(invalid, 30, *replacement); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Invalid boundary mapping accepted");
        std::cout << "Installed consumer: matching, generation, port reconnection and 48 output checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
