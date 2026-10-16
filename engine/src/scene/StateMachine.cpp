#include <utility>

#include <ntype/engine/scene/StateMachine.hpp>

namespace ntype::engine::scene {

void StateMachine::push(std::unique_ptr<IState> state) {
    pending_.push_back({Operation::Push, std::move(state)});
}

void StateMachine::pop() {
    pending_.push_back({Operation::Pop, nullptr});
}

void StateMachine::replace(std::unique_ptr<IState> state) {
    pending_.push_back({Operation::Replace, std::move(state)});
}

void StateMachine::clear() {
    pending_.push_back({Operation::Clear, nullptr});
}

void StateMachine::apply_pending() {
    auto operations = std::exchange(pending_, {});
    for (auto& [operation, state] : operations) {
        const bool removes_top = operation != Operation::Push;
        if (operation == Operation::Clear) {
            while (!stack_.empty()) {
                stack_.back()->on_exit();
                stack_.pop_back();
            }
            continue;
        }
        if (removes_top && !stack_.empty()) {
            stack_.back()->on_exit();
            stack_.pop_back();
        }
        if (state) {
            stack_.push_back(std::move(state));
            stack_.back()->on_enter();
        }
    }
}

void StateMachine::handle_event(const platform::InputEvent& event) {
    if (IState* state = current()) {
        state->handle_event(event);
    }
}

void StateMachine::update(const float dt) {
    apply_pending();
    if (IState* state = current()) {
        state->update(dt);
    }
}

void StateMachine::render(platform::IRenderer& renderer, const float alpha) {
    if (IState* state = current()) {
        state->render(renderer, alpha);
    }
}

IState* StateMachine::current() const noexcept {
    return stack_.empty() ? nullptr : stack_.back().get();
}

} // namespace ntype::engine::scene
