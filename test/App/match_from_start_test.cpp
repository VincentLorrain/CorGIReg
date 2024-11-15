#include <catch2/catch_test_macros.hpp>

#include "BasicGraphIr/Graph.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"
#include "NodeTest/NodeDictionary.hpp"

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

            "A->B->C",
            "A->B->C->D",
            "A#->B;A#->C",
            "A#->B;A#->C",
            "A#->B+;A#->C*",

            "(A#->B)*",
            "(A#->B)+",
            "(A#->B->C#)*",
        };
        /*
        somme nodes type
        */
        NodeDictionary::registerFunction("A", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "A"; });
        NodeDictionary::registerFunction("B", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "B"; });
        NodeDictionary::registerFunction("C", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "C"; });
        NodeDictionary::registerFunction("D", [](NodeNN::Ptr node) { return node->getInfo<std::string>("Type") == "D"; });
        
        
        for (auto query : querys) {
            auto transactionGraph = PGInterpreterTG(query).interpret();
            auto graph = PGInterpreterIR(query).interpret();
            
            PermutationIterator<std::shared_ptr<NodeNN>> combIt(graph->getNodes(), transactionGraph->getNbStart());

            bool found = false;
            while (combIt.hasNext()) {
                auto start = combIt.next();
                auto match = transactionGraph->test(start);
                if (match->isMatch()) {
                    found = true;
                    break;

                }
            }
            INFO(query);
            REQUIRE(found);
        }

    }

}//TEST_CASE