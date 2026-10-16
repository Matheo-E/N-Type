#pragma once

#include <ntype/engine/platform/IRenderer.hpp>
#include <ntype/engine/platform/InputEvent.hpp>

namespace ntype::engine::scene {

/// One screen or phase of an application (menu, game, game over...).
class IState {
public:
    virtual ~IState() = default;

    virtual void on_enter() {}
    virtual void on_exit() {}
    virtual void handle_event(const platform::InputEvent& event) = 0;
    /// Called at the fixed simulation rate.
    virtual void update(float dt) = 0;
    virtual void render(platform::IRenderer& renderer, float alpha) = 0;
};

} // namespace ntype::engine::scene
