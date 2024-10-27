#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
// TODO try catch NOOOOO


using namespace CorGIReg; 

PartitionGraphParser::PartitionGraphParser(const std::string gRegexExpressions):
mLexer(gRegexExpressions)
{
    mCurrentToken = mLexer.getNextToken();
}

PartitionGraphParser::~PartitionGraphParser() noexcept = default;


const std::string PartitionGraphParser::getQuery(){
    return mLexer.getQuery();
}

std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::parse(void){

    std::shared_ptr<AstNode<tGTokensTypes>> astTree = constructAstAllExpr();
    rstParser();
    return astTree;
}


void PartitionGraphParser::rstParser(void){
    mLexer.rstPosition();
    mCurrentToken = mLexer.getNextToken();
}


void PartitionGraphParser::ackToken(tGTokensTypes  tokenType){

    if(mCurrentToken->getType() == tokenType ){
        try {
            mCurrentToken = mLexer.getNextToken();
        } catch (const std::runtime_error& e) {
            std::ostringstream errorMessage;
            errorMessage << "Graph Lexer error in Parser :\n"<< e.what() << std::endl;
            throw std::runtime_error(errorMessage.str());
        }
    }else{
        std::ostringstream errorMessage;
        errorMessage << "Bad syntax PartitionGraphParser " << static_cast<int>(mCurrentToken->getType())  <<"!="<< static_cast<int>(tokenType) << "\n";
        errorMessage << mLexer.rep();
        throw std::runtime_error(errorMessage.str());
    }
}

/*
exp : KEY(QOM | QZM | QZO)?  | CKEY | SCKEY | domain
*/
std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::constructAstExp(void)
{

    try{
        std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
        std::shared_ptr<AstNode<tGTokensTypes>> node = std::make_shared<AstNode<tGTokensTypes>>(token);

        if (mCurrentToken->getType() == tGTokensTypes::KEY  ){
            ackToken(tGTokensTypes::KEY );
            if (mCurrentToken->getType() == tGTokensTypes::QOM  ){
                token = mCurrentToken->copy();
                ackToken(tGTokensTypes::QOM );
                std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
                return newNode;
            }else if (mCurrentToken->getType() == tGTokensTypes::QZM  ){
                token = mCurrentToken->copy();
                ackToken(tGTokensTypes::QZM );
                std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
                return newNode;
            }else if (mCurrentToken->getType() == tGTokensTypes::QZO  ){
                token = mCurrentToken->copy();
                ackToken(tGTokensTypes::QZO );
                std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
                return newNode;
            }
            return node;
        }else if (mCurrentToken->getType() == tGTokensTypes::CKEY){
            ackToken(tGTokensTypes::CKEY );
            return node;
        }else if (mCurrentToken->getType() == tGTokensTypes::SCKEY){
            ackToken(tGTokensTypes::SCKEY );
            return node;
        }else{
            return constructAstDomain();
        }

    } catch (const std::runtime_error& e) {
        std::ostringstream errorMessage;
        errorMessage << "PartitionGraphParser constructAstExp :\n"<< e.what() << std::endl;
        throw std::runtime_error(errorMessage.str());
    }
}

/*
seq : (exp (((NEXT seq)* | NEXT END ) )| (exp|END) (PREC rseq)* 
*/
std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::constructAstSeq(void)
{
   try{
        std::shared_ptr<AstNode<tGTokensTypes>> left = nullptr;
        if(mCurrentToken->getType() != tGTokensTypes::END){
            left = constructAstExp();

            if(mCurrentToken->getType() == tGTokensTypes::NEXT )
            {
                std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
                ackToken(tGTokensTypes::NEXT);

                if(mCurrentToken->getType() == tGTokensTypes::END )
                {
                    std::shared_ptr<ParsingToken<tGTokensTypes>> tokenEnd = mCurrentToken->copy();
                    ackToken(tGTokensTypes::END);
                    return std::make_shared<AstNode<tGTokensTypes>>(token,
                    std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,std::make_shared<AstNode<tGTokensTypes>>(tokenEnd)});
                }

                std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,constructAstSeq()});
                left = newNode;

            }
            
            else if(mCurrentToken->getType() == tGTokensTypes::PREC )
            {
                std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
                ackToken(tGTokensTypes::PREC);

                std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,constructAstRseq()});
                left = newNode;
            }

        }else{
            std::shared_ptr<ParsingToken<tGTokensTypes>> tokenEnd = mCurrentToken->copy();
            ackToken(tGTokensTypes::END);
            left = std::make_shared<AstNode<tGTokensTypes>>(tokenEnd);
            std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
            ackToken(tGTokensTypes::PREC);
            std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,constructAstRseq()});
            left = newNode;

        }   
        return left;

    } catch (const std::runtime_error& e) {
        std::ostringstream errorMessage;
        errorMessage << "PartitionGraphParser constructAstSeq :\n"<< e.what() << std::endl;
        throw std::runtime_error(errorMessage.str());
    }

}

/*
rseq : exp ((PREC rseq)* )
*/
std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::constructAstRseq(void){

    try{
        std::shared_ptr<AstNode<tGTokensTypes>> left = constructAstExp();

        if(mCurrentToken->getType() == tGTokensTypes::PREC){
            std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
            ackToken(tGTokensTypes::PREC);

            if(mCurrentToken->getType() == tGTokensTypes::END )
            {
                std::shared_ptr<ParsingToken<tGTokensTypes>> tokenEnd = mCurrentToken->copy();
                ackToken(tGTokensTypes::END);
                return std::make_shared<AstNode<tGTokensTypes>>(token,
                std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,std::make_shared<AstNode<tGTokensTypes>>(tokenEnd)});
            }
            std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,constructAstRseq()});
            left = newNode;

        }
        return left;

    } catch (const std::runtime_error& e) {
        std::ostringstream errorMessage;
        errorMessage << "PartitionGraphParser constructAstRseq :\n"<< e.what() << std::endl;
        throw std::runtime_error(errorMessage.str());
    }
}

/*
LPAREN seq RPAREN (QOM | QZM | QZO)
*/
std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::constructAstDomain(void)
{

   try{
        std::shared_ptr<ParsingToken<tGTokensTypes>> token ;
        std::shared_ptr<AstNode<tGTokensTypes>> node ;

        token = mCurrentToken->copy();
        ackToken(tGTokensTypes::LPAREN);
        node = std::make_shared<AstNode<tGTokensTypes>>(token,
        std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{constructAstSeq()});
        ackToken(tGTokensTypes::RPAREN);
        //(QOM | QZM)

        token = mCurrentToken->copy();
        if (mCurrentToken->getType() == tGTokensTypes::QOM){
            ackToken(tGTokensTypes::QOM);
            node = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
        }else if (mCurrentToken->getType() == tGTokensTypes::QZM){
            ackToken(tGTokensTypes::QZM);
            node = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
        }else if (mCurrentToken->getType() == tGTokensTypes::QZO){
            ackToken(tGTokensTypes::QZO);
            node = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{node});
        }else{
            std::ostringstream errorMessage;
            errorMessage << "Bad syntax constructAstDomain must have quantifier \n";
            throw std::runtime_error(errorMessage.str());
        }

        return node;

    } catch (const std::runtime_error& e) {
        std::ostringstream errorMessage;
        errorMessage << "PartitionGraphParser constructAstDomain :\n"<< e.what() << std::endl;
        throw std::runtime_error(errorMessage.str());
    }
}

/*
allExpr: seq (SEP allExpr)* | STOP
*/
std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::constructAstAllExpr(void)
{

    try{
        std::shared_ptr<AstNode<tGTokensTypes>> left = constructAstSeq();
        if(mCurrentToken->getType() == tGTokensTypes::SEP )
        {
            std::shared_ptr<ParsingToken<tGTokensTypes>> token = mCurrentToken->copy();
            ackToken(tGTokensTypes::SEP);

            if(mCurrentToken->getType() == tGTokensTypes::STOP )
            {
                 return left;
            }
            std::shared_ptr<AstNode<tGTokensTypes>> newNode = std::make_shared<AstNode<tGTokensTypes>>(token,
            std::vector<std::shared_ptr<AstNode<tGTokensTypes>>>{left,constructAstAllExpr()});
            left = newNode;
        }else//in others case token must be STOP
        {
            ackToken(tGTokensTypes::STOP);
        }
        return left;

    } catch (const std::runtime_error& e) {
        std::ostringstream errorMessage;
        errorMessage << "PartitionGraphParser constructAstAllExpr :\n"<< e.what() << std::endl;
        throw std::runtime_error(errorMessage.str());
    }
}