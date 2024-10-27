#include "TransitionGraph/MatchSolution.hpp"


namespace CorGIReg {

MatchSolution::MatchSolution(std::vector<std::shared_ptr<RunTimeContext>>& precedence,
                             const std::string& query,
                             const std::vector<NodePtr>& startNode)
    : mQueryFrom(query), mStartNode(startNode) {
    //TODO
    //ASSERT all precedence must be compatible
    //ASSERT all precedence must be valid

    // Reformat the solution
    for (const auto& context : precedence) {
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

const std::set<NodePtr> MatchSolution::getAll() const {
    // Create a unique set to store all the elements
    std::set<NodePtr> uniqueSet;

    // Iterate through the map and insert elements from each set into the unique set
    for (const auto& pair : mSolution) {
        uniqueSet.insert(pair.second.begin(), pair.second.end());
    }

    return uniqueSet;
}

bool MatchSolution::areCompatible(const std::shared_ptr<MatchSolution>& solution) const {
    std::set<NodePtr> set1 = solution->getAll();
    std::set<NodePtr> set2 = getAll();
    std::set<NodePtr> intersection;
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                          std::inserter(intersection, intersection.begin()));
    return intersection.empty();
}


/***
 * 
 */

    SolutionFromStartingPoint::SolutionFromStartingPoint(const std::vector<std::shared_ptr<RunTimeContext>> validRunTime, 
    const std::set<std::size_t> allGraphOrigin,const std::string query,const std::vector<NodePtr> startNode):
    mAllGraphOrigin(allGraphOrigin),mValidRunTime(validRunTime),mQuery(query),mStartNode(startNode){
        std::vector<std::shared_ptr<RunTimeContext>> precedence;
        _makeTheMatch(precedence);
    }

    void SolutionFromStartingPoint::_makeTheMatch( std::vector<std::shared_ptr<RunTimeContext>>& precedence){
        const std::set<std::size_t> actOrigin = _getOrigins(precedence);

        
        if (mAllGraphOrigin == actOrigin) {

            // We have a set of RunTimeContext that are compatible
            // and cover all the graph origin (in others words, all the sub query)

            std::set<NodePtr> validNode;
            for (const auto& contextPtr : precedence) {
                std::set<std::shared_ptr<NodeNN>>  tmp =  contextPtr->getValidNodes();

                validNode.insert(tmp.begin(), tmp.end());
            }
            std::set<NodePtr> rejectNode;
            for (const auto& contextPtr : precedence) {
                std::set<std::shared_ptr<NodeNN>>  tmp = contextPtr->getRejectedNodes();
                rejectNode.insert(tmp.begin(),tmp.end());
            }

            if (std::includes(validNode.begin(), validNode.end(), rejectNode.begin(), rejectNode.end())) {
                mMatches.push_back(std::make_shared<MatchSolution>(precedence, mQuery, mStartNode));
            }

            precedence.pop_back();
            return;
        }

        for (const auto& oneOfAll : mValidRunTime) {
            if (precedence.empty()) {
            //if (oneOfAll->getActState()->getOrigin().empty()) {
                precedence.push_back(oneOfAll);
                _makeTheMatch(precedence);
            }else{
                /* add only usfull solution that valid at leas one new origin */
                // Warning is maybe  strict intersection
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
            precedence.pop_back();
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
