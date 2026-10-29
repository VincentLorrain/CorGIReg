#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/Common/CommonKey.hpp"
#include <set>
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

namespace {
using Ast = std::shared_ptr<AstNode<tGTokensTypes>>;
using T = tGTokensTypes;

bool quantifier(T type) { return type == T::QOM || type == T::QZM || type == T::QZO; }
bool anchor(T type) { return type == T::CKEY || type == T::SCKEY; }
// Units of a sequence in traversal order: PREC places its right operand first.
void units(const Ast& node, std::vector<Ast>& out) {
    if (node->getType() != T::NEXT && node->getType() != T::PREC) { out.push_back(node); return; }
    units(node->getChilds()[node->getType() == T::PREC], out);
    units(node->getChilds()[node->getType() == T::NEXT], out);
}
Ast body(const Ast& group) {
    const auto& child = group->getChilds()[0];
    return child->getType() == T::LPAREN ? child->getChilds()[0] : child;
}
bool nullable(const Ast& node) {
    const auto type = node->getType();
    if (type == T::KEY || anchor(type)) return false;
    if (type == T::QZM || type == T::QZO || type == T::END) return true;
    if (type == T::QOM) return nullable(body(node));
    std::vector<Ast> list;
    units(node, list);
    for (const auto& unit : list) if (!nullable(unit)) return false;
    return true;
}
bool mandatory(const Ast& node) { return node->getType() != T::END && !nullable(node); }
bool containsAnchor(const Ast& node) {
    if (anchor(node->getType())) return true;
    for (const auto& child : node->getChilds()) if (containsAnchor(child)) return true;
    return false;
}
std::string identity(const Ast& node) { return parseCommonKey(node->getValue()).identity(); }

// Static boundary rules: '$' must follow a guaranteed operation, cannot end an
// iteration that the next one continues, and an anchor that may end at '$'
// cannot have a mandatory successor elsewhere.
class BoundaryCheck {
    std::set<std::string> terminal, followed;
    [[noreturn]] static void illegal(const std::string& message) {
        throw std::invalid_argument("Illegal boundary: " + message);
    }
    // Anchors that can be the last operation of a unit.
    void lastAnchors(const Ast& node, std::set<std::string>& out) {
        if (anchor(node->getType())) { out.insert(identity(node)); return; }
        if (!quantifier(node->getType())) return;
        std::vector<Ast> list;
        units(body(node), list);
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            lastAnchors(*it, out);
            if (mandatory(*it)) break;
        }
    }
public:
    // guaranteed: a mandatory operation precedes this sequence in its path.
    // chained: inside a repeated group without anchor, whose iterations follow each other.
    // optional: inside a ?/* group, so its operations are not mandatory.
    // successor: a mandatory operation follows this sequence.
    void sequence(const Ast& node, bool guaranteed, bool chained, bool optional, bool successor) {
        std::vector<Ast> list;
        units(node, list);
        std::vector<bool> after(list.size() + 1, successor);
        for (std::size_t i = list.size(); i-- > 0;) after[i] = after[i + 1] || mandatory(list[i]);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto& unit = list[i];
            const auto type = unit->getType();
            if (type == T::END) {
                if (chained) illegal("'$' inside a repeated group without anchor, as in (A->$)+");
                if (!guaranteed) illegal("no operation is guaranteed before '$', as in B?->$");
                for (std::size_t j = i; j-- > 0;) {
                    lastAnchors(list[j], terminal);
                    if (mandatory(list[j])) break;
                }
            } else if (anchor(type)) {
                if (!optional && after[i + 1]) followed.insert(identity(unit));
            } else if (quantifier(type)) {
                const bool repeated = type != T::QZO && !containsAnchor(unit);
                sequence(body(unit), guaranteed, chained || repeated, optional || type != T::QOM, after[i + 1]);
            }
            guaranteed = guaranteed || mandatory(unit);
        }
    }
    void paths(const Ast& node) {
        if (node->getType() == T::SEP) {
            for (const auto& child : node->getChilds()) paths(child);
            return;
        }
        sequence(node, false, false, false, false);
    }
    void finish() const {
        for (const auto& name : terminal)
            if (followed.count(name))
                illegal("anchor " + name + " ends at '$' but has a mandatory successor, as in A#->$;A#->B");
    }
};
}

std::shared_ptr<AstNode<tGTokensTypes>> PartitionGraphParser::parse(void){

    std::shared_ptr<AstNode<tGTokensTypes>> astTree = constructAstAllExpr();
    rstParser();
    BoundaryCheck check;
    check.paths(astTree);
    check.finish();
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