#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <memory>
#include <algorithm> // For std::set_intersection

#include "NodeTest/NodeDictionary.hpp"
#include "BasicGraphIr/Graph.hpp"
#include "TransitionGraph/RunTimeContext.hpp"

namespace CorGIReg {

using NodePtr = std::shared_ptr<NodeNN>; // Assuming Node is defined elsewhere


/**
 * @brief The MatchSolution class is used to store the solution of the matching wen a set of RunTimeContext are valid compatible
 * there repesante a match
 * 
 */
class MatchSolution {
private:
    std::map<std::string, std::set<NodePtr>> mSolution;
    const std::string mQueryFrom;
    const std::vector<NodePtr> mStartNode;

public:
    MatchSolution() = delete;

    MatchSolution(std::vector<std::shared_ptr<RunTimeContext>>& precedence,
                  const std::string& query,
                  const std::vector<NodePtr>& startNode);

    inline const std::set<NodePtr>& at(const std::string& key) const {
        return mSolution.at(key);
    }

    const std::set<NodePtr> getAll() const;

    bool areCompatible(const std::shared_ptr<MatchSolution>& solution) const;

    inline const std::string& getQuery() const noexcept { return mQueryFrom; }
    inline const std::vector<NodePtr>& getStartNode() const noexcept { return mStartNode; }
};

/**
 * @brief The SolutionFromStartingPoint class is used to store the solution of the matching wen a set of RunTimeContext from a given start node
 */
class SolutionFromStartingPoint
{
private:
    /**
     * @brief The all valid RunTimeContext from the starting point
     */
    const std::vector<std::shared_ptr<RunTimeContext>> mValidRunTime;
    /**
     * @brief The query
     */
    const std::string mQuery;

    /**
     * @brief The start node
     */
    const std::vector<NodePtr> mStartNode;
    /**
     * @brief The all graph origin
     */
    const std::set<std::size_t> mAllGraphOrigin;

    /** */
    std::vector<std::shared_ptr<MatchSolution>> mMatches;

public:
    SolutionFromStartingPoint(const std::vector<std::shared_ptr<RunTimeContext>> validRunTime, const std::set<std::size_t> allGraphOrigin,const std::string query,const std::vector<NodePtr> startNode);
    ~SolutionFromStartingPoint() = default;
    bool isMatch() const{
        return !mMatches.empty();
    }

private:
    void _makeTheMatch( std::vector<std::shared_ptr<RunTimeContext>>& precedence);
    const std::set<std::size_t> _getOrigins(std::vector<std::shared_ptr<RunTimeContext>>& precedence);

};


} // namespace CorGIReg


