#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"
#include "TransitionGraph/TransitionTrace.hpp"


namespace CorGIReg{

PGInterpreterTG::PGInterpreterTG(const std::string graphMatchExpr):mParser(graphMatchExpr){
    mActGroupe = 0;
}

std::shared_ptr<TGCompiled>  PGInterpreterTG::interpret(void){
    mActGroupe = 0;
    std::shared_ptr<AstNode<tGTokensTypes>> tree = mParser.parse();
    mRootSelectionPlan = RootSelectionPass{}.run(tree);
    mQuantifierIndices.clear();
    indexQuantifiers(tree);
    return visit(tree)->compile();
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree){

    std::vector<std::shared_ptr<AstNode<tGTokensTypes>>> nextAstNodes = AstTree->getChilds();

    if(AstTree->getType() == tGTokensTypes::SEP){
        return sepF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::NEXT){
        return nextF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::PREC){
        return precF(visit(nextAstNodes[0]),visit(nextAstNodes[1]));
    }else if(AstTree->getType() == tGTokensTypes::QOM){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '+');
    }else if(AstTree->getType() == tGTokensTypes::QZM){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '*');
    }else if(AstTree->getType() == tGTokensTypes::QZO){
        return quantify(visit(nextAstNodes[0]), mQuantifierIndices.at(AstTree.get()), '?');
    }else if(AstTree->getType() == tGTokensTypes::KEY || AstTree->getType() == tGTokensTypes::CKEY || AstTree->getType() == tGTokensTypes::SCKEY){
        return keyF(AstTree);
    }else if (AstTree->getType() == tGTokensTypes::END){
        return endF(AstTree);
    }else if(AstTree->getType() == tGTokensTypes::LPAREN){
        mActGroupe += 1;
        std::shared_ptr<TransitionGraph> out = visit(nextAstNodes[0]);
        mActGroupe -= 1;
        return out;
    }else{
        throw std::logic_error("visit Bad token type" );
    }
}


std::shared_ptr<TransitionGraph> PGInterpreterTG::keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*leaf 
        K -> V
    */
    if (AstNode->getType() == tGTokensTypes::KEY){
        
        return std::make_shared<TransitionGraph>( TransitionTypes::UNIQUE,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);

    }else if(AstNode->getType() == tGTokensTypes::CKEY){

        return std::make_shared<TransitionGraph>( TransitionTypes::COMMON,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);

    }else if(AstNode->getType() == tGTokensTypes::SCKEY){
        return std::make_shared<TransitionGraph>( TransitionTypes::STRICTCOMMON,
                                                AstNode->getValue(), mParser.getQuery(),mActGroupe);
    }else{

        throw std::logic_error("keyF Bad in AST" );
    }
    return nullptr;
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode){
    /*
        Null
        S -> V
    */
    return std::make_shared<TransitionGraph>( TransitionTypes::NNULL,
                                            AstNode->getValue(), mParser.getQuery(),mActGroupe);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::sepF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){

    /*
    The idea is to have an identifier for each query, which allows us to verify that in the end all queries have a solution
    */

   /*
    The problem with merging, particularly the merging of valid states,
    is that we lose the origin of the query, which invalidates the solutions. 
    Two solutions are then possible : 

    - The first is to ensure that a state has multiple queries
    we can maintain a link between the TG and the query

    - The second possibility is to replace the 2 query indexes with a new one
    
    */
    leftTG->incOriginFrom(rightTG);
    return leftTG->unionG(rightTG);
    //return leftTG->fusionG(rightTG);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::nextF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           A    B
        SA -> q -> VB
    */
    return leftTG->mergeOneStartOneValid(rightTG);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::precF(std::shared_ptr<TransitionGraph> leftTG,std::shared_ptr<TransitionGraph> rightTG){
    /*
        combine the 2 Graph
        all valid node of A are  merge with Start B, Start B is un Start
        update the relative reference  

           A          B
        SA -> VA + SB -> VB
           B    A
        SB -x> q -x> VA
    */
    // For the PRECEDENCE operator (<-), the resulting graph should match
    // a predecessor relationship.  The left graph represents the node that
    // is preceded, while the right graph represents the preceding node.
    // We simply merge the valid state of the left graph with the start of
    // the right graph without flipping the transition directions.
    // This keeps the transitions directed from the preceding node to the
    // preceded node.
    return rightTG->mergeOneStartOneValid(leftTG);
}

void PGInterpreterTG::indexQuantifiers(const std::shared_ptr<AstNode<tGTokensTypes>>& node) {
    // Postorder follows lexical symbol order: inner postfixes precede outer ones.
    for (const auto& child : node->getChilds()) indexQuantifiers(child);
    const auto type = node->getType();
    if (type == tGTokensTypes::QOM || type == tGTokensTypes::QZM || type == tGTokensTypes::QZO)
        mQuantifierIndices.emplace(node.get(), mQuantifierIndices.size() + 1);
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::quantify(
    std::shared_ptr<TransitionGraph> body, std::size_t index, char symbol) {
    const auto starts = body->getStartStates();
    const auto ends = body->getValidStates();
    if (starts.size() != 1 || ends.size() != 1)
        throw std::logic_error("Quantifier requires a single entry and exit");
    auto first = starts.front();
    auto last = *ends.begin();
    auto entry = std::make_shared<State>(false, true);
    auto completed = std::make_shared<State>(false, false);
    auto exit = std::make_shared<State>(true, false);
    for (const auto& state : {entry, completed, exit}) state->setGroupe(mActGroupe);
    first->unStart();
    last->unValid();
    auto trace = [&](auto from, auto to, TraceAction action) {
        auto edge = std::make_shared<TransitionTrace>(from, to, action, index, symbol);
        edge->updateWeak();
        body->addTransition(edge);
    };
    trace(entry, first, TraceAction::Begin);
    trace(last, completed, TraceAction::Iteration);
    trace(completed, exit, TraceAction::End);
    if (symbol != '?') {
        auto type = body->containCommonTransition() ? TransitionTypes::REF : TransitionTypes::EMPTY;
        body->addTransition(TransitionFactory::make(completed, first, type));
    }
    if (symbol != '+') trace(entry, exit, TraceAction::Skip);
    return body;
}

}
