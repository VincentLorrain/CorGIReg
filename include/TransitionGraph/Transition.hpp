
#pragma once

#include "GraphAccess/GraphView.hpp"
#include "TransitionGraph/State.hpp"
#include "TransitionGraph/RunTimeContext.hpp"


#include <atomic>
#include <set>
#include <sstream>
#include <memory>
#include <map>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <fmt/core.h>

#include <regex> //for factory
#include "TransitionGraph/State.hpp"
#include "TransitionGraph/RunTimeContext.hpp"

/// An edge of the pattern machine. A successful transition updates the walk
/// context and supplies candidate source positions for the destination state.
namespace CorGIReg{
    class State;
    class RunTimeContext;


    /// Transition categories used by the compiler and runtime.
    enum class TransitionTypes {
        EMPTY = 0,
        REF,
        COMMON,
        STRICTCOMMON,
        UNIQUE,
        NNULL,
        NONE /**< Reject-all transition; not built by TransitionFactory. */
    };

    struct TransitionResult {
        bool success;
        NodeIds node;
    };


    class Transition: public std::enable_shared_from_this<Transition> {
        static std::atomic<std::size_t> sID;
        const std::size_t mId = sID.fetch_add(1, std::memory_order_relaxed);

    protected:
        // Legacy relative-position metadata propagated when edges are rewired.
        // Repetition restarts use TransitionRef's separate body analysis.
        std::map<size_t,int> mRelativeToCommon;

        // A transition owns its two endpoint states.
        std::shared_ptr<State> mStateSource;
        std::shared_ptr<State> mStateDest;

        // Retained by the legacy constructor. State back-links are also weak.
        std::weak_ptr<Transition> weakPtr;


        // Whether consuming a node moves the cursor to its children or parents.
        bool mDirSourceToDest = true;

        // Predicate key resolved against the request's matching environment.

        const std::string mNodeTestKey;

    public:
        /// Store the endpoints and predicate key. Call updateWeak after shared
        /// ownership is established to register the endpoint links.
        Transition(std::shared_ptr<State> source,std::shared_ptr<State> dest, const std::string odeTestKey);
        virtual  ~Transition(){};
        Transition() : weakPtr(shared_from_this()) {}       

        /// Evaluate this edge against a branch-local context. A successful
        /// result with no candidates represents an absent source position.

        virtual const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) =0;
        std::size_t getId() const noexcept { return mId; }

        /**
         * @brief kind of the transition, to switch on instead of casting
         * @details subclasses of TransitionEmpty (port and trace actions) report EMPTY
         */
        virtual TransitionTypes getType(void) const =0;

        /// Return whether this edge consumes and binds a common anchor.
        virtual bool isCommon(void){ 
            return false; 
        }

        /// Return the anchor identity index, or the maximum index for other edges.
        virtual size_t getCommonIdx(void){
            return std::numeric_limits<std::size_t>::max();
        }
        /// Expose the legacy relative distances keyed by anchor identity.
        inline const std::map<size_t,int>& getRelative(void){
            return mRelativeToCommon;
        }
        /// Add relative distances that this transition does not already store.
        void updateRelative( const std::map<size_t,int>& relativePos );
        /**
         * @brief get source FsmNode
         * @return FsmNode
        */
        inline const std::shared_ptr<State>& getSourceState(void){
            return mStateSource;
        }

          /**
         * @brief get dest FsmNode
         * @return FsmNode
        */
        inline const std::shared_ptr<State>& getDestState(void){
            return mStateDest;
        }
        /**
         * @brief set a new dest to the edge
         * @return FsmNode
        */
        void reSetDestState(const std::shared_ptr<State> newDest);
        /**
         * @brief set a new source to the edge
         * @return FsmNode
        */
        void reSetSourceState(const std::shared_ptr<State> newSource);

        /// Propagate legacy relative-position metadata between adjacent transitions.
        void propagateRelativePos(void);
        /// Register weak outgoing and incoming links at the endpoint states.
        void updateWeak(void);

        /**
         * @brief invert the next node direction node -> children or node -> parents 
        */
       void flipDir(void){
           mDirSourceToDest = !mDirSourceToDest;
       }
       /**
         * @brief test if the next node is a child
        */
       inline bool isDirectionIsChildren(void){
           return mDirSourceToDest;
       }

       const std::string getTestKey(void){
            return mNodeTestKey;
       }
       virtual std::string strRep(void) const;

    };


    /**
     * @brief class specialization for unique node validation transition st0 --A--> st1
     */
    class TransitionUnique:public Transition
    {
        public:
        TransitionUnique(std::shared_ptr<State> source,std::shared_ptr<State> dest, const std::string odeTestKey);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context)  override;
        TransitionTypes getType(void) const override { return TransitionTypes::UNIQUE; }
        std::string strRep(void) const override;
    };

    /**
     * @enum CommonMode 
     * @brief Select which neighbors a common-node occurrence requires the solution to cover.
     */
    enum class CommonMode {
        NONE = 0, /**< no node are rejected (the validation on the others child of parent of common node is not mandatory)*/
        FRONT,/** need to valid all the node in the same direction of the visit  */
        BACK,/** need to valid all the node in the opposite direction of the visit  */
        ALL, /**< Every parent and child must be covered. */
        INPUTS, /**< Absolute graph parents, independent of traversal direction. */
        OUTPUTS /**< Absolute graph children, independent of traversal direction. */
    };

     /**
     * @brief Consume a node and bind it to a common identity such as A#.
     * @see Transition
    */
    class TransitionCommon:public Transition
    {

        private:
        /**
         * @brief Process-wide mapping from normalized anchor keys to identity indices.
        */
        static std::map<std::string,std::size_t> mCommonIdxMap;
        /**
         * @brief the common id test in this transition
        */
        std::size_t mCommonIdx;
        /**
         * @brief Coverage requirement for this occurrence of the anchor.
        */
        CommonMode mFlag;
        public:

        /**
         * @brief Resolve the normalized anchor key to its shared identity index.
        */
        TransitionCommon(std::shared_ptr<State> source,std::shared_ptr<State> dest, const std::string nodeTestKey,
        const std::string commonKey,CommonMode flag);

 
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        TransitionTypes getType(void) const override {
            return mFlag == CommonMode::NONE ? TransitionTypes::COMMON : TransitionTypes::STRICTCOMMON;
        }
        bool isCommon(void) override {
            return true;
        }
        size_t getCommonIdx(void) override { return mCommonIdx; }

        std::string strRep(void) const override;

    };



    /**
     * @brief class specialization for ref transition no test just go back in the operation graph
     * @see  Transition
    */
    class TransitionRef:public Transition
    {
        public:
        // Distances from one common anchor back to the body start
        // (+1 per parent-directed body step, -1 per child-directed step).
        // Distances beyond kLimit make that side unbounded.
        struct Anchor {
            std::set<int> deltas;
            bool unboundedParents = false, unboundedChildren = false;
        };
        static constexpr int kLimit = 32;

        private:
        struct Body {
            std::map<std::size_t, Anchor> anchors;
            bool canSkipAnchors = false;
        };
        const Body mBody;

        public:
        /**
         * @brief constructor
         * @param source source state of the edge
         * @param dest dest state of the edge
         */
        TransitionRef(std::shared_ptr<State> source,std::shared_ptr<State> dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        TransitionTypes getType(void) const override { return TransitionTypes::REF; }
        std::string strRep(void) const override;

        private:
        static Body analyzeBody(const std::shared_ptr<State>& start, const std::shared_ptr<State>& completed);

    };

    /**
     * @brief class specialization for ref empty transition accept all test 
     * @see  Transition
    */
    class TransitionEmpty:public Transition
    {
        public:
        TransitionEmpty(std::shared_ptr<State> source,std::shared_ptr<State> dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        TransitionTypes getType(void) const override { return TransitionTypes::EMPTY; }
        std::string strRep(void) const override;
    };


    /**
     * @brief class specialization for undefine transition reject all 
     * @see  Transition
    */
    class TransitionNone:public Transition
    {
        public:
        TransitionNone(std::shared_ptr<State> source,std::shared_ptr<State> dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        TransitionTypes getType(void) const override { return TransitionTypes::NONE; }
        std::string strRep(void) const override;

    };

    /**
     * @brief class specialization for valid a nullptr , use to valid the end of Graph
     * @see  Transition
    */
    class TransitionNull:public Transition
    {
        public:
        TransitionNull(std::shared_ptr<State> source,std::shared_ptr<State> dest);
        const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override;
        TransitionTypes getType(void) const override { return TransitionTypes::NNULL; }
        std::string strRep(void) const override;
    };


/*
factory
 */




class TransitionFactory {
    public:
    /**
    * @brief factory for making edge and read the info in the lexeme of the token
    * @param source source node of the edge
    * @param dest Dest node of the edge
    * @param type type of the edge
    * @param lexeme the additional information to build the edge
    * @return s prt of the edge
    */
    static std::shared_ptr<Transition> make(std::shared_ptr<State> source,std::shared_ptr<State> dest,TransitionTypes type,const std::string lexeme = "");
   };




} // namespace CorGIReg
