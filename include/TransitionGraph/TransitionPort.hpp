#pragma once
#include "TransitionGraph/Transition.hpp"
namespace CorGIReg {
enum class PortAction { Begin, Check, End };
class TransitionPort final : public TransitionEmpty {
    PortAction action;
    PortNumber port;
public:
    TransitionPort(std::shared_ptr<State> source, std::shared_ptr<State> dest,
                   PortAction action, PortNumber port)
        : TransitionEmpty(source, dest), action(action), port(port) {}
    const TransitionResult test(const std::shared_ptr<RunTimeContext>& context) override {
        if (action == PortAction::Begin) context->beginPortScope();
        if (action == PortAction::Check && !context->testInputPort(port)) return {false, {}};
        if (action == PortAction::End && !context->endPortScope()) return {false, {}};
        auto node = context->getActNode();
        return {true, node ? NodeIds{*node} : NodeIds{}};
    }
    std::string strRep() const override { return "input-port " + std::to_string(port); }
};
}
