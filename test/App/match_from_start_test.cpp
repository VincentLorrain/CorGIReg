#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <vector>

#include "BasicGraphIr/Graph.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "NodeTest/NodeDictionary.hpp"
#include "Grexgex/RandomExpressionGenerator.h"

#include "Utile/Utile.hpp"

using namespace CorGIReg;


TEST_CASE("APP") {


    SECTION("test match from start (A#->B)*") {
        /*
        definition of the nodes type
         */
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "A"; });
        NodeDictionary::registerFunction("B", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "B"; });
        /*
        Definition of the graph (A#->B)*
         */
        auto graph = std::make_shared<GraphNN>();

        auto nodeA = std::make_shared<NodeNN>();
        auto nodeB = std::make_shared<NodeNN>();
        auto nodeB2 = std::make_shared<NodeNN>();

        nodeA->setInfo("Type", std::string("A"));
        nodeB->setInfo("Type", std::string("B"));
        nodeB2->setInfo("Type", std::string("B"));

        graph->addNode(nodeA);
        graph->addNode(nodeB);
        graph->addNode(nodeB2);

        graph->addEdge(nodeA, nodeB);
        graph->addEdge(nodeA, nodeB2);

        /*
        Gregex definition
        */
        auto transactionGraph = PGInterpreterTG("(A#->B)*").interpret();
        std::vector<std::shared_ptr<NodeNN>> start =  {nodeA};
        auto match = transactionGraph->test(start);
        REQUIRE(match->isMatch());
    }

    SECTION("test match from start auto test") {
        
        std::vector<std::string> querys = {
            "A->B",

            "A<-B",

            "A->B->C",
            "A->B->C->D",
            "A#->B;A#->C",
            "A$->B;A$->C",
            "A#->B+;A#->C*",

            "(A#->B)*",
            "(A#->B)+",
            "(A#->B->C#)*",

            "A#->B;A#->B",
        };
        /*
        somme nodes type
        */
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "A"; });
        NodeDictionary::registerFunction("B", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "B"; });
        NodeDictionary::registerFunction("C", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "C"; });
        NodeDictionary::registerFunction("D", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "D"; });
        
        
        for (auto query : querys) {
            INFO("QUERY");
            INFO(query);
            INFO("\n");

            auto transactionGraph = PGInterpreterTG(query).interpret();
            auto graph = PGInterpreterIR(query).interpret();
            std::size_t nbStart = transactionGraph->getNbStart();
            INFO("NB START");
            INFO(nbStart);
            INFO("\n");
            auto nodes = graph->getNodes();
            INFO("NB NODES");
            INFO(nodes.size());
            INFO("\n");
            PermutationIterator<std::shared_ptr<NodeNN>> combIt(nodes, nbStart);

            bool found = false;
            while (combIt.hasNext()) {
                auto starts = combIt.next();
                auto match = transactionGraph->test(starts);
                if (match->isMatch()) {
                    found = true;
                    break;
                }
            }

            INFO(transactionGraph->exportToMermaid());
            INFO("\n");
            INFO(graph->exportToMermaid());
            INFO("\n");
            REQUIRE(found);
        }

    }



    SECTION(" auto app test") {
        

        /*
        somme nodes type
        */
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "A"; });
        NodeDictionary::registerFunction("B", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "B"; });
        NodeDictionary::registerFunction("C", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "C"; });
        NodeDictionary::registerFunction("D", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "D"; });
        

        RandomExpressionGenerator::setDictionary({"A", "B", "C", "D"});
        
        for (int i = 0; i < 10; ++i) {

            auto query =  RandomExpressionGenerator::allExpr();

            INFO("QUERY");
            INFO(query);
            INFO("\n");
            
            auto graph = PGInterpreterIR(query).interpret();
            auto transactionGraph = PGInterpreterTG(query).interpret();
            
            std::size_t nbStart = transactionGraph->getNbStart();
            INFO("NB START");
            INFO(nbStart);
            INFO("\n");
            auto nodes = graph->getNodes();
            INFO("NB NODES");
            INFO(nodes.size());
            INFO("\n");
            PermutationIterator<std::shared_ptr<NodeNN>> combIt(nodes, nbStart);

            bool found = false;
            while (combIt.hasNext()) {
                auto starts = combIt.next();
                auto match = transactionGraph->test(starts);
                if (match->isMatch()) {
                    found = true;
                    break;
                }
            }

            INFO(transactionGraph->exportToMermaid());
            INFO("\n");
            INFO(graph->exportToMermaid());
            INFO("\n");
            REQUIRE(found);
        }

        RandomExpressionGenerator::clearDictionary();

    }

}//TEST_CASE