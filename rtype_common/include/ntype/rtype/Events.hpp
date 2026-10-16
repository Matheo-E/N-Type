#pragma once

#include <cstdint>

#include <ntype/engine/ecs/Entity.hpp>
#include <ntype/rtype/Components.hpp>

namespace ntype::rtype {

using engine::ecs::Entity;

/// Two colliders overlap (published once per pair and tick).
struct CollisionEvent {
    Entity first;
    Entity second;
};

/// An enemy was destroyed by damage.
struct EnemyKilled {
    Entity enemy;
    std::uint8_t killer_slot = Projectile::kEnemyOwner;
    std::uint32_t score_value = 0;
};

struct PlayerDied {
    std::uint8_t slot = 0;
    std::uint8_t lives_left = 0;
};

struct PlayerRespawned {
    std::uint8_t slot = 0;
};

/// Published by the cleanup system right before the entity is destroyed.
struct EntityDestroyed {
    Entity entity;
    std::uint32_t network_id = 0;
    DestroyReason reason = DestroyReason::Despawned;
};

} // namespace ntype::rtype
