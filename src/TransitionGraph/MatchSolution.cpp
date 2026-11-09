#include "TransitionGraph/MatchSolution.hpp"
#include "TransitionGraph/State.hpp"


namespace CorGIReg {

MatchSolution::MatchSolution(std::vector<std::shared_ptr<RunTimeContext>>& precedence,
                             const std::string& query,
                             const std::vector<std::optional<NodeId>>& startNode, std::shared_ptr<const GraphView> graph)
    : mGraph(std::move(graph)), mQueryFrom(query), mStartNode(startNode) {
    // The caller has already checked walk compatibility and coverage.
    // Preserve each ordered trace while indexing captures by predicate.
    for (const auto& context : precedence) {
        mWalks.push_back({context->getSubStmId(), context->getCaptures(), context->getQuantifiers()});
        for (const auto& pair : context->getValid()) {
            auto key = pair.first;
            if (mSolution.find(key) == mSolution.end()) {
                mSolution[key] = pair.second;
            } else {
                mSolution[key].insert(pair.second.begin(), pair.second.end());
            }
        }
    }
}

const std::set<NodeId> MatchSolution::getAll() const {
    std::set<NodeId> uniqueSet;

    for (const auto& pair : mSolution) {
        uniqueSet.insert(pair.second.begin(), pair.second.end());
    }

    return uniqueSet;
}

bool MatchSolution::areCompatible(const std::shared_ptr<MatchSolution>& solution) const {
    if (mGraph != solution->mGraph) return true;
    std::set<NodeId> set1 = solution->getAll();
    std::set<NodeId> set2 = getAll();
    std::set<NodeId> intersection;
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                          std::inserter(intersection, intersection.begin()));
    return intersection.empty();
}


    SolutionFromStartingPoint::SolutionFromStartingPoint(const std::vector<std::shared_ptr<RunTimeContext>> validRunTime, 
    const std::set<std::size_t> allGraphOrigin,const std::string query,const std::vector<std::optional<NodeId>> startNode, std::shared_ptr<const GraphView> graph):
    mGraph(std::move(graph)),
    mAllGraphOrigin(allGraphOrigin),mValidRunTime(validRunTime),mQuery(query),mStartNode(startNode){
        std::vector<std::shared_ptr<RunTimeContext>> precedence;
        _makeTheMatch(precedence);
    }

    void SolutionFromStartingPoint::_makeTheMatch( std::vector<std::shared_ptr<RunTimeContext>>& precedence){
        const std::set<std::size_t> actOrigin = _getOrigins(precedence);

        
        if (mAllGraphOrigin == actOrigin) {

            // These compatible walks cover every path origin. Check strict
            // obligations against the union of their captured source IDs.

            std::set<NodeId> validNode;
            for (const auto& contextPtr : precedence) {
                std::set<NodeId>  tmp =  contextPtr->getValidNodes();

                validNode.insert(tmp.begin(), tmp.end());
            }
            std::set<NodeId> rejectNode;
            for (const auto& contextPtr : precedence) {
                std::set<NodeId>  tmp = contextPtr->getRejectedNodes();
                rejectNode.insert(tmp.begin(),tmp.end());
            }

            if (std::includes(validNode.begin(), validNode.end(), rejectNode.begin(), rejectNode.end())) {
                mMatches.push_back(std::make_shared<MatchSolution>(precedence, mQuery, mStartNode, mGraph));
            }

            if (!precedence.empty()) precedence.pop_back();
            return;
        }

        // Walks are combined in one canonical order: each step covers the
        // lowest path not covered yet, so a set of walks yields one solution.
        std::size_t next = 0;
        for (auto origin : mAllGraphOrigin) if (!actOrigin.count(origin)) { next = origin; break; }
        for (const auto& oneOfAll : mValidRunTime) {
            if (!oneOfAll->getActState()->getOrigin().count(next)) continue;
            if (precedence.empty()) {
                precedence.push_back(oneOfAll);
                _makeTheMatch(precedence);
            }else{
                // A new walk must cover an uncovered origin without reusing a
                // path already represented by another walk in this solution.
                std::vector<std::size_t> intersection;
                std::set_intersection(oneOfAll->getActState()->getOrigin().begin(), oneOfAll->getActState()->getOrigin().end(),
                          actOrigin.begin(), actOrigin.end(),
                          std::back_inserter(intersection));

                bool compatible = true;
                for (const auto& context: precedence) {
                    if(!context->areCompatible(oneOfAll)){
                        compatible = false;
                        break;
                    }
                }

                if (intersection.empty() && compatible) {
                    precedence.push_back(oneOfAll);
                    _makeTheMatch(precedence);
                }
            }


        }

        if (!precedence.empty()) {
            if (!precedence.empty()) precedence.pop_back();
        }
        return;
    }

    const std::set<std::size_t> SolutionFromStartingPoint::_getOrigins(std::vector<std::shared_ptr<RunTimeContext>>& precedence){
        std::set<std::size_t> out;
        for (const auto& context : precedence) {
            out.insert(context->getActState()->getOrigin().begin(), 
            context->getActState()->getOrigin().end());
        }
        return out;
    }



} // namespace CorGIReg
