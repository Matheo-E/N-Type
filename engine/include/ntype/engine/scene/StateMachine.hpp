#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <ntype/engine/scene/IState.hpp>

namespace ntype::engine::scene {

/// Stack of states. Only the top one receives events and updates.
///
/// push/pop/replace are deferred until `apply_pending()` (called at the start
/// of `update`), so a state can safely request its own replacement.
class StateMachine {
public:
    void push(std::unique_ptr<IState> state);
    void pop();
    void replace(std::unique_ptr<IState> state);
    void clear();

    void apply_pending();

    void handle_event(const platform::InputEvent& event);
    void update(float dt);
    void render(platform::IRenderer& renderer, float alpha);

    [[nodiscard]] IState* current() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return stack_.empty() && pending_.empty(); }
    [[nodiscard]] std::size_t depth() const noexcept { return stack_.size(); }

private:
    enum class Operation { Push, Pop, Replace, Clear };
    struct Pending {
        Operation operation;
        std::unique_ptr<IState> state;
    };

    std::vector<std::unique_ptr<IState>> stack_;
    std::vector<Pending> pending_;
};

} // namespace ntype::engine::scene
