#pragma once
#include "TransitionGraph/Transition.hpp"

namespace CorGIReg {

enum class TraceAction { Begin, Iteration, End, Skip };

// Metadata epsilon: never consumes a source node, including at graph end.
// Inheriting TransitionEmpty lets anchor resolution recognize zero distance.
class TransitionTrace final : public TransitionEmpty {
    TraceAction mAction;
    std::size_t mIndex;
    char mSymbol;
public:
    TransitionTrace(std::shared_ptr<State> source, std::shared_ptr<State> dest,
                    TraceAction action, std::size_t index, char symbol)
        : TransitionEmpty(source, dest), mAction(action), mIndex(index), mSymbol(symbol) {}
    const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override {
        switch (mAction) {
        case TraceAction::Begin: context->beginQuantifier(mIndex, mSymbol); break;
        case TraceAction::Iteration:
            if (!context->completeIteration(mIndex)) return {false, {}};
            break;
        case TraceAction::End: context->endQuantifier(mIndex); break;
        case TraceAction::Skip:
            context->beginQuantifier(mIndex, mSymbol);
            context->endQuantifier(mIndex);
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
