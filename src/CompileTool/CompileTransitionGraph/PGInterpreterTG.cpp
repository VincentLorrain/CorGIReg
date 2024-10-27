#include "CompileTool/CompileTransitionGraph/PGInterpreterTG.hpp"


namespace CorGIReg{

PGInterpreterTG::PGInterpreterTG(const std::string graphMatchExpr):mParser(graphMatchExpr){
    mActGroupe = 0;
}

std::shared_ptr<TGCompiled>  PGInterpreterTG::interpret(void){
    mActGroupe = 0;
    std::shared_ptr<AstNode<tGTokensTypes>> tree = mParser.parse();
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
    return rightTG->setTransitionChildToParent()->mergeOneStartOneValid(leftTG->setTransitionChildToParent());
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::qomF(std::shared_ptr<TransitionGraph> tg){
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
    if (tg->containCommonTransition()){
        return tg->addTransitionValidToStart(TransitionTypes::REF,"");
    }else{
        return tg->addTransitionValidToStart(TransitionTypes::EMPTY,"");
    }
}

std::shared_ptr<TransitionGraph> PGInterpreterTG::qzmF(std::shared_ptr<TransitionGraph> tg){
    /*
    qomf and a bypass empty start to valid
    */
    return qzoF(qomF(tg));

}

std::shared_ptr<TransitionGraph> PGInterpreterTG::qzoF(std::shared_ptr<TransitionGraph> tg){
    /*
    quantification zero or one
    just connect the start to the valid
     */
    return tg->addTransitionStartToValid(TransitionTypes::EMPTY,"");
}

}