#pragma once

#include <cstdint>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/math/Random.hpp>
#include <ntype/rtype/GameData.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::rtype {

using engine::ecs::Entity;
using engine::ecs::Registry;

/// Builds every kind of entity from GameData, so client and server agree on
/// what a "bydos" or a "player missile" is.
class EntityFactory {
public:
    EntityFactory(Registry& registry, const GameData& data) noexcept
        : registry_(registry), data_(data) {}

    // Server side: network ids are allocated here.
    Entity create_player(std::uint8_t slot);
    Entity create_enemy(std::uint8_t enemy_type, Vec2f position, engine::math::Random& rng);
    /// Fires the owner's weapon. `direction` is normalised.
    Entity create_missile(Entity owner, Vec2f direction);

    // Client side: mirrors an entity announced by the server.
    Entity create_from_spawn(const protocol::SpawnedEntity& spawned);
    /// Purely visual, never networked.
    Entity create_explosion(Vec2f position);

    /// Puts a player back at its start position after a death.
    void respawn_player(Entity player);
    [[nodiscard]] Vec2f player_start(std::uint8_t slot) const noexcept;

    void reset_network_ids() noexcept { next_network_id_ = 1; }

private:
    void add_visual(Entity entity, const SpriteDef& sprite, int layer);
    Entity create_player_entity(std::uint8_t slot, std::uint32_t network_id);
    Entity create_enemy_entity(std::uint8_t enemy_type, Vec2f position, std::uint32_t network_id);
    Entity create_missile_entity(EntityKind kind, std::uint8_t projectile, Vec2f position,
                                 Vec2f direction, std::uint8_t owner_slot,
                                 std::uint32_t network_id);

    Registry& registry_;
    const GameData& data_;
    std::uint32_t next_network_id_ = 1;
};

} // namespace ntype::rtype
