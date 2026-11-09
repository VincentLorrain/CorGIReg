#pragma once



#include <string>
#include <vector>
#include <map>
#include <memory>


#include "TransitionGraph/TransitionGraph.hpp"
#include "CompileTool/Common/AstNode.hpp"
#include "CompileTool/Common/Repetition.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphLexer.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include "CompileTool/CompileTransitionGraph/PartitionGraphTokens.hpp"
#include "CompileTool/PostAstOptimization/RootSelectionPass.hpp"

namespace CorGIReg {

    /// Compile a parsed pattern into the state machine used for matching.
    class PGInterpreterTG
    {
    private:
        PartitionGraphParser mParser;
        std::size_t mActGroupe;
        const std::string mGraphMatchExpr;
        RootSelectionPlan mRootSelectionPlan;
        std::map<const AstNode<tGTokensTypes>*, std::size_t> mQuantifierIndices;
        void indexQuantifiers(const std::shared_ptr<AstNode<tGTokensTypes>>& node);
        std::shared_ptr<TransitionGraph> quantify(std::shared_ptr<TransitionGraph> body,
                                               std::size_t index, const Repetition& repetition);
        // One entry and one exit, in the enclosing group, joined to every
        // alternative by empty transitions. Alternatives stay one path.
        std::shared_ptr<TransitionGraph> alternate(const std::vector<std::shared_ptr<TransitionGraph>>& branches);
        // Visits a parenthesized group one level deeper; an alternation is
        // assembled back at the enclosing level.
        std::shared_ptr<TransitionGraph> group(const std::shared_ptr<AstNode<tGTokensTypes>>& paren, bool reverse);

    public:
        PGInterpreterTG(const std::string graphMatchExpr);
        virtual ~PGInterpreterTG() =default;

        /// Compile each path in traversal order, starting from its first operand.
        std::shared_ptr<TGCompiled>  interpret(void);
        // Same matches, searched from anchor roots: each component whose
        // selected root is a common anchor present in every one of its paths
        // (without numbered arrows or boundaries) is split at that anchor into a
        // forward part and a reversed part walking to parents. All parts start
        // on the anchor and share one start group (TGCompiled::getStartGroups),
        // so captures are listed from the anchor outward. Other paths compile
        // as interpret() does.
        std::shared_ptr<TGCompiled>  interpretRooted(void);
        const RootSelectionPlan& getRootSelectionPlan() const { return mRootSelectionPlan; }

        private:

        std::shared_ptr<TransitionGraph> visit(std::shared_ptr<AstNode<tGTokensTypes>> AstTree);

        /// Compile a single plain, common, or strict predicate transition.
        std::shared_ptr<TransitionGraph> keyF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /// Compile a boundary transition that requires an absent graph position.
        std::shared_ptr<TransitionGraph> endF(std::shared_ptr<AstNode<tGTokensTypes>> AstNode);

        /// Keep two paths separate and give their walks distinct origins.
        std::shared_ptr<TransitionGraph> sepF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);
        /// Join the end of the left machine to the start of the right machine.
        std::shared_ptr<TransitionGraph> nextF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);

        /// Join the right machine before the left machine for a reverse arrow.
        std::shared_ptr<TransitionGraph> precF(std::shared_ptr<TransitionGraph> leftFsm,std::shared_ptr<TransitionGraph> rightFsm);
        // A path unit traversed from its end to its start, towards parents.
        std::shared_ptr<TransitionGraph> reversed(std::shared_ptr<AstNode<tGTokensTypes>> AstTree);
            

    };



}



