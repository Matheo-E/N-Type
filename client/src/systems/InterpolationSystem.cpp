#include "InterpolationSystem.hpp"

#include <algorithm>

#include <ntype/engine/ecs/Registry.hpp>

#include "../ClientComponents.hpp"

namespace ntype::client {

namespace {

constexpr float kMaxExtrapolation = 0.1F;

} // namespace

void InterpolationSystem::update(engine::ecs::Registry& registry, const float dt) {
    registry.view<Interpolated, rtype::Transform>().each(
        [dt](engine::ecs::Entity, Interpolated& smoothing, rtype::Transform& transform) {
            smoothing.elapsed += dt;
            if (smoothing.duration <= 0.0F) {
                return;
            }
            if (smoothing.elapsed <= smoothing.duration) {
                transform.position = engine::math::lerp(smoothing.from, smoothing.to,
                                                        smoothing.elapsed / smoothing.duration);
                return;
            }
            const float overdue =
                std::min(smoothing.elapsed - smoothing.duration, kMaxExtrapolation);
            transform.position = smoothing.to + smoothing.velocity * overdue;
        });
}

} // namespace ntype::client
