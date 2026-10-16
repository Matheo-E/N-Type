#pragma once

// R-Type components. Plain aggregates: all the logic lives in systems.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <ntype/engine/math/Rect.hpp>
#include <ntype/engine/math/Vec2.hpp>
#include <ntype/engine/platform/Color.hpp>

namespace ntype::rtype {

using engine::math::Recti;
using engine::math::Vec2f;

enum class EntityKind : std::uint8_t { Player, Enemy, PlayerMissile, EnemyMissile };
inline constexpr std::uint8_t kEntityKindCount = 4;

enum class DestroyReason : std::uint8_t { Despawned, Killed, Hit, Expired };
inline constexpr std::uint8_t kDestroyReasonCount = 4;

namespace input_bits {
inline constexpr std::uint8_t kUp = 1U << 0U;
inline constexpr std::uint8_t kDown = 1U << 1U;
inline constexpr std::uint8_t kLeft = 1U << 2U;
inline constexpr std::uint8_t kRight = 1U << 3U;
inline constexpr std::uint8_t kFire = 1U << 4U;
inline constexpr std::uint8_t kAll = kUp | kDown | kLeft | kRight | kFire;
} // namespace input_bits

namespace collision_layers {
inline constexpr std::uint8_t kPlayer = 1U << 0U;
inline constexpr std::uint8_t kEnemy = 1U << 1U;
inline constexpr std::uint8_t kPlayerShot = 1U << 2U;
inline constexpr std::uint8_t kEnemyShot = 1U << 3U;
} // namespace collision_layers

struct Transform {
    Vec2f position;
};

struct Velocity {
    Vec2f linear;
};

/// Axis-aligned box centred on the Transform position.
struct Collider {
    Vec2f size;
    std::uint8_t layer = 0;
    /// Layers this collider reacts to.
    std::uint8_t mask = 0;
};

struct Health {
    std::int32_t current = 1;
    std::int32_t max = 1;
};

/// Damage dealt to whatever this entity touches.
struct Damage {
    std::int32_t amount = 1;
};

struct Lifetime {
    float remaining = 0.0F;
};

struct Weapon {
    float cooldown = 0.25F;
    float since_last_shot = 0.0F;
    std::uint8_t projectile = 0;
    bool trigger_held = false;
};

/// Identity shared by the server and every client.
struct NetworkId {
    std::uint32_t id = 0;
};

/// What the entity is, as announced in EntitySpawn.
struct EntityType {
    EntityKind kind = EntityKind::Enemy;
    std::uint8_t subtype = 0;
};

struct PlayerInput {
    std::uint8_t bits = 0;
    std::uint32_t client_tick = 0;
};

struct Player {
    std::uint8_t slot = 0;
    std::uint8_t lives = 3;
    bool alive = true;
    float respawn_timer = 0.0F;
    float invulnerable = 0.0F;
};

struct Score {
    std::uint32_t points = 0;
};

struct Projectile {
    static constexpr std::uint8_t kEnemyOwner = 0xFF;
    /// Player slot that fired it, or kEnemyOwner.
    std::uint8_t owner_slot = kEnemyOwner;
};

enum class BehaviorType : std::uint8_t { Straight, Sine };

struct EnemyBehavior {
    BehaviorType type = BehaviorType::Straight;
    float speed = 100.0F;
    float amplitude = 0.0F;
    float frequency = 0.0F;
    float phase = 0.0F;
    Vec2f origin;
    std::uint32_t score_value = 0;
};

/// Marks an entity for removal at the end of the tick.
struct PendingDestroy {
    DestroyReason reason = DestroyReason::Despawned;
};

struct Sprite {
    std::string texture;
    Recti source;
    float scale = 1.0F;
    int layer = 0;
    engine::platform::Color tint = engine::platform::colors::kWhite;
};

struct Animation {
    std::vector<Recti> frames;
    float frame_time = 0.1F;
    float elapsed = 0.0F;
    std::size_t current = 0;
    bool loop = true;
};

} // namespace ntype::rtype
