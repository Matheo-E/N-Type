#include "WorldSync.hpp"

#include "ClientComponents.hpp"
#include "ClientEvents.hpp"

namespace ntype::client {

namespace protocol = rtype::protocol;

engine::ecs::Entity WorldSync::find(const std::uint32_t network_id) const noexcept {
    const auto it = by_network_id_.find(network_id);
    return it == by_network_id_.end() ? engine::ecs::kNullEntity : it->second;
}

void WorldSync::apply(const protocol::EntitySpawn& message) {
    for (const auto& spawned : message.entities) {
        // Reliable messages may be delivered twice: spawning is idempotent.
        if (registry_.is_alive(find(spawned.network_id))) {
            continue;
        }
        const engine::ecs::Entity entity = factory_.create_from_spawn(spawned);
        // The server decides when replicas disappear; drop local gameplay timers.
        registry_.remove<rtype::Lifetime>(entity);
        registry_.remove<rtype::Weapon>(entity);
        const auto position = registry_.get<rtype::Transform>(entity).position;
        registry_.emplace<Interpolated>(entity, position, position, rtype::Vec2f{}, 0.0F, 0.0F);
        registry_.emplace<ReplicaFlags>(entity);
        by_network_id_[spawned.network_id] = entity;
        events_.publish(ReplicaSpawned{spawned.kind, position});
    }
}

void WorldSync::apply(const protocol::EntityDestroy& message) {
    for (const auto& destroyed : message.entities) {
        const auto it = by_network_id_.find(destroyed.network_id);
        if (it == by_network_id_.end()) {
            continue;
        }
        const engine::ecs::Entity entity = it->second;
        by_network_id_.erase(it);
        if (!registry_.is_alive(entity)) {
            continue;
        }
        const auto* type = registry_.try_get<rtype::EntityType>(entity);
        const auto position = registry_.get<rtype::Transform>(entity).position;
        events_.publish(ReplicaDestroyed{type != nullptr ? type->kind : rtype::EntityKind::Enemy,
                                         position, destroyed.reason});
        registry_.destroy(entity);
    }
}

void WorldSync::apply(const protocol::WorldSnapshot& message) {
    // Snapshots are unreliable and can arrive late: never go back in time.
    // Equal ticks are accepted because big worlds are split over several snapshots.
    if (message.server_tick < last_tick_) {
        return;
    }
    last_tick_ = message.server_tick;
    for (const auto& state : message.entities) {
        const engine::ecs::Entity entity = find(state.network_id);
        if (!registry_.is_alive(entity)) {
            continue;
        }
        const rtype::Vec2f target{protocol::dequantize_position(state.x),
                                  protocol::dequantize_position(state.y)};
        auto& interpolated = registry_.get<Interpolated>(entity);
        interpolated.from = registry_.get<rtype::Transform>(entity).position;
        interpolated.to = target;
        interpolated.velocity = {static_cast<float>(state.vx), static_cast<float>(state.vy)};
        interpolated.elapsed = 0.0F;
        interpolated.duration = snapshot_period_;
        if (auto* health = registry_.try_get<rtype::Health>(entity)) {
            health->current = state.hp;
        }
        auto& flags = registry_.get<ReplicaFlags>(entity);
        const bool was_hidden = flags.hidden;
        flags.hidden = (state.flags & protocol::entity_flags::kHidden) != 0;
        flags.invulnerable = (state.flags & protocol::entity_flags::kInvulnerable) != 0;
        if (was_hidden && !flags.hidden) {
            // Respawn: jump instead of sliding across the screen.
            interpolated.from = target;
            registry_.get<rtype::Transform>(entity).position = target;
        }
    }
}

} // namespace ntype::client
