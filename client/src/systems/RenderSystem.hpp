#pragma once

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/platform/IRenderer.hpp>

namespace ntype::client {

/// Draws every entity with a Sprite, back to front by layer. Not an ISystem:
/// it runs in the render pass, not in the fixed-rate simulation.
class RenderSystem {
public:
    explicit RenderSystem(engine::platform::IRenderer& renderer) noexcept : renderer_(renderer) {}

    void draw(engine::ecs::Registry& registry, float time);

private:
    engine::platform::IRenderer& renderer_;
};

} // namespace ntype::client
