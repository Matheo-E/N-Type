#pragma once

#include <ntype/engine/ecs/ISystem.hpp>

namespace ntype::client {

/// Moves replicas smoothly towards their latest snapshot position, then
/// keeps them going with their last velocity for a short while to hide a
/// lost snapshot.
class InterpolationSystem final : public engine::ecs::ISystem {
public:
    void update(engine::ecs::Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Interpolation"; }
};

} // namespace ntype::client
