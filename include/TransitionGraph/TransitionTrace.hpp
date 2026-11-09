#pragma once
#include "CompileTool/Common/Repetition.hpp"
#include "TransitionGraph/Transition.hpp"

namespace CorGIReg {

enum class TraceAction { Begin, Iteration, End, Skip, Repeat };

// Metadata epsilon: never consumes a source node, including at graph end.
// Inheriting TransitionEmpty lets anchor resolution recognize zero distance.
class TransitionTrace final : public TransitionEmpty {
    TraceAction mAction;
    std::size_t mIndex;
    Repetition mRepetition;
public:
    TransitionTrace(std::shared_ptr<State> source, std::shared_ptr<State> dest,
                    TraceAction action, std::size_t index, char symbol)
        : TransitionTrace(source, dest, action, index, Repetition{0, std::nullopt, symbol, {}}) {}
    TransitionTrace(std::shared_ptr<State> source, std::shared_ptr<State> dest,
                    TraceAction action, std::size_t index, Repetition repetition)
        : TransitionEmpty(source, dest), mAction(action), mIndex(index), mRepetition(std::move(repetition)) {}
    const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override {
        switch (mAction) {
        case TraceAction::Begin:
            context->beginQuantifier(mIndex, mRepetition.symbol, mRepetition.notation);
            break;
        case TraceAction::Iteration:
            if (!context->completeIteration(mIndex, mRepetition.minimum, mRepetition.maximum.has_value()))
                return {false, {}};
            break;
        case TraceAction::End:
            if (!mRepetition.allows(context->quantifierCount(mIndex))) return {false, {}};
            context->endQuantifier(mIndex);
            break;
        case TraceAction::Skip:
            context->beginQuantifier(mIndex, mRepetition.symbol, mRepetition.notation);
            context->endQuantifier(mIndex);
            break;
        case TraceAction::Repeat:
            if (mRepetition.maximum && context->quantifierCount(mIndex) >= *mRepetition.maximum)
                return {false, {}};
            break;
        }
        auto node = context->getActNode();
        return {true, node ? NodeIds{*node} : NodeIds{}};
    }
    std::string strRep() const override {
        return std::to_string(mStateSource->getId()) + "-->|q" + std::to_string(mIndex) +
               ":" + std::to_string(static_cast<int>(mAction)) + "|" + std::to_string(mStateDest->getId());
    }
};

} // namespace CorGIReg
