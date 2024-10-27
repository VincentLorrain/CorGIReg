#include "CompileTool/CompileTransitionGraph/PGInterpreterIR.hpp"


namespace CorGIReg{

PGInterpreterIR::PGInterpreterIR(const std::string graphMatchExpr):mParser(graphMatchExpr){
    mActGroupe = 0;
}


std::shared_ptr<GraphNN>  PGInterpreterIR::interpret(void){
    mActGroupe = 0;
    std::shared_ptr<AstNode<tGTokensTypes>> tree = mParser.parse();
    std::shared_ptr<GraphNN> out = visit(tree);

    // combine the common nodes
    for (auto common : mCommonNodes){   
        out->mergeNodes(common.second);
    }

    return out;
}



std::shared_ptr<GraphNN> PGInterpreterIR::unionIR(std::shared_ptr<GraphNN>& leftTG,std::shared_ptr<GraphNN>& rightTG){
    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
    }
    return leftTG;
}

void PGInterpreterIR::deleteIR(std::shared_ptr<GraphNN>& tg){

    // Iterate over all nodes in the task graph 'tg'
    for (auto& node : tg->getNodes()) {
        // Retrieve the key from the node's information
        auto nodeKey = node->getInfo<std::string>("Key");

        // Find the key in the map
        auto mapIt = mCommonNodes.find(nodeKey);
        if (mapIt != mCommonNodes.end()) {
            // We found the key, now remove the node from the corresponding set
            auto& nodeSet = mapIt->second;
            auto setIt = nodeSet.find(node);
            if (setIt != nodeSet.end()) {
                nodeSet.erase(setIt); // Safely erase using the iterator
            }
        }

        // Reset the smart pointer if needed
        //node.reset(); 
    }
    //tg.reset();
}

std::shared_ptr<GraphNN> PGInterpreterIR::visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree){

    std::vector<std::shared_ptr<AstNode<tGTokensTypes>>> nextAstNodes = AstTree->getChilds();

    if(AstTree->getType() == tGTokensTypes::SEP){
        return sepF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::NEXT){
        return nextF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::PREC){
        return precF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::QOM){
        return qomF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::QZM){
        return qzmF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::QZO){
        return qzoF(visit(nextAstNodes[0]));
    }else if(AstTree->getType() == tGTokensTypes::KEY || AstTree->getType() == tGTokensTypes::CKEY || AstTree->getType() == tGTokensTypes::SCKEY){
        return keyF(AstTree);
    }else if (AstTree->getType() == tGTokensTypes::END){
        return endF(AstTree);
    }else if(AstTree->getType() == tGTokensTypes::LPAREN){
        mActGroupe += 1;
        std::shared_ptr<GraphNN> out = visit(nextAstNodes[0]);
        mActGroupe -= 1;
        return out;
    }else{
        throw std::logic_error("visit Bad token type" );
    }
}


std::shared_ptr<GraphNN> PGInterpreterIR::keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*leaf 
        K -> V
    */

    auto graph =  std::make_shared<GraphNN>();
    std::shared_ptr<NodeNN> node;
    std::string lexeme = AstNode->getValue();
    node = std::make_shared<NodeNN>();
    /*
    the key in the query
    */
    node->setInfo("Key",lexeme);

    if (AstNode->getType() == tGTokensTypes::KEY){
        node->setInfo("Type",lexeme);
    }else if(AstNode->getType() == tGTokensTypes::CKEY || AstNode->getType() == tGTokensTypes::SCKEY ){
        node->setInfo("isCommon",true);

        std::regex combinedCommonKey("([A-Za-z_0-9]+)[#$][0-9]*");
        std::smatch matches;
        if (std::regex_match(lexeme, matches, combinedCommonKey)){
            node->setInfo("Type",matches[1].str());
        }else{
            throw std::logic_error("combinedCommonKey ERROR" );
        }

        // add the node to the lis of common nodes to be merge at the end 
        if (mCommonNodes.find(lexeme) == mCommonNodes.end()){
                mCommonNodes[lexeme] = std::set<std::shared_ptr<NodeNN>>({node});
        }else{
            mCommonNodes[lexeme].insert(node);
        }
   
    }else{

        throw std::logic_error("keyF Bad in AST" );
    }
    
    graph->addNode(node);
    return graph;

}

std::shared_ptr<GraphNN> PGInterpreterIR::endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*
        Null
        S -> V
    */
    return std::make_shared<GraphNN>();
}

std::shared_ptr<GraphNN> PGInterpreterIR::sepF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    for (auto rNode : rightTG->getNodes()){
            leftTG->addNode(rNode);
    }

    
    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::nextF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    /*
        combine the 2 Graph
        get the node that have no child in leftTG
        and connect to the node with no parent in rightTG
    */


    for (auto lNode : leftTG->getNodes()){
        for (auto rNode : rightTG->getNodes()){
            if (rNode->getParents().size() == 0 && 
                lNode->getChildren().size() == 0){
                leftTG->addEdge(lNode,rNode);
            }
        }
    }

    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
    }
    rightTG = unionIR(rightTG,leftTG);

    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::precF(std::shared_ptr<GraphNN> leftTG,std::shared_ptr<GraphNN> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           B    A
        SB -x> q -x> VA
    */
    for (auto rNode : rightTG->getNodes()){
        leftTG->addNode(rNode);
   }

    for (auto lNode : leftTG->getNodes()){
        for (auto rNode : rightTG->getNodes()){
            if (rNode->getParents().size() == 0 && 
                lNode->getChildren().size() == 0){
                leftTG->addEdge(rNode,lNode);
            }
        }
    }

    return leftTG;
}

std::shared_ptr<GraphNN> PGInterpreterIR::qomF(std::shared_ptr<GraphNN> tg){
    /*
        +
        valid node is connect to the child of Start with the same edge condition
            A
        S -> V

            A
        S -> V
          (E|R)
        V -> S
    */
   //todo
    bool hasCommon = false;
    for (auto node : tg->getNodes()){
        if (node->hasArgument("isCommon")){
            hasCommon = true;
            break;
        }
    }

    auto qzmfPart = qzmF(tg);

    if (!hasCommon){
        return nextF(tg,qzmfPart);
    }
    return unionIR(tg,qzmfPart); 

}

std::shared_ptr<GraphNN> PGInterpreterIR::qzmF(std::shared_ptr<GraphNN> tg){
    /*
    have zero of more instance of the graph, is like dublicate the graph 
    and combine it a rdm between 0 and N time the graph
    */
    

    std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
    // Define the maximum value for N as size_t
    // Create a distribution from 0 to N (inclusive), using size_t
    std::uniform_int_distribution<size_t> dist(0, mMaxLoops);
    // Generate a random number of iterations
    size_t randomIterations = dist(rng);



    std::shared_ptr<GraphNN> newTg = std::make_shared<GraphNN>();

    for (size_t i = 0; i < randomIterations -1; i++) {
        std::shared_ptr<GraphNN> tgFixed = tg->deepCopy({});
        /*
        add the common nodes to the list of common nodes
        */
        bool hasCommon = false;
        for (auto node : tgFixed->getNodes()){
            if (node->hasArgument("isCommon")){
                const std::string key = node->getInfo<std::string>("Key");
                hasCommon = true;
                if (mCommonNodes.find(key) == mCommonNodes.end()){
                    mCommonNodes[key] = std::set<std::shared_ptr<NodeNN>>({node});
                }else{
                    mCommonNodes[key].insert(node);
                }
            }
        }

        if (!hasCommon){
            //it's a sequence of nodes just concat it 
            newTg = nextF(newTg,tgFixed);
        }else{
            newTg = unionIR(newTg,tgFixed); 
        }
    }
        
    /* it's important to delete the old graph because of the common nodes save in the map*/
    deleteIR( tg);

    return newTg;

}

std::shared_ptr<GraphNN> PGInterpreterIR::qzoF(std::shared_ptr<GraphNN> tg){
    /*
    quantification zero or one
    just connect the start to the valid
     */

    std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));

    // Create a distribution that covers only 0 and 1
    std::uniform_int_distribution<int> dist(0, 1);

    // Generate a random binary value
    int randomBinary = dist(rng);

    if (randomBinary == 0) {
        return tg;

    } else {
        deleteIR(tg);
        return std::make_shared<GraphNN>();

    }
}


}//namespace CorGIReg