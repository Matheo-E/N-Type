#pragma once

// Simulation systems shared by the server (authoritative) and, for the
// purely cosmetic ones, the client. Each system does one thing; they talk
// through components and the EventBus, never by calling each other.

#include <cstddef>
#include <vector>

#include <ntype/engine/ecs/ISystem.hpp>
#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/events/EventBus.hpp>
#include <ntype/engine/math/Random.hpp>
#include <ntype/rtype/EntityFactory.hpp>
#include <ntype/rtype/Events.hpp>
#include <ntype/rtype/GameData.hpp>

namespace ntype::rtype::systems {

using engine::ecs::ISystem;
using engine::ecs::Registry;
using engine::events::EventBus;

/// Turns each player's input bits into a velocity and a trigger state.
class PlayerControlSystem final : public ISystem {
public:
    explicit PlayerControlSystem(const GameData& data) noexcept : data_(data) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "PlayerControl"; }

private:
    const GameData& data_;
};

/// Movement patterns of the Bydos.
class EnemyBehaviorSystem final : public ISystem {
public:
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "EnemyBehavior"; }
};

/// Fires a missile whenever a held trigger's cooldown is over. Enemies aim
/// at the closest living player.
class WeaponSystem final : public ISystem {
public:
    explicit WeaponSystem(EntityFactory& factory) noexcept : factory_(factory) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Weapon"; }

private:
    EntityFactory& factory_;
};

class MovementSystem final : public ISystem {
public:
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Movement"; }
};

/// Keeps players on screen and removes anything that left it.
class BoundsSystem final : public ISystem {
public:
    explicit BoundsSystem(Vec2f world_size) noexcept : world_size_(world_size) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Bounds"; }

private:
    Vec2f world_size_;
};

/// Publishes a CollisionEvent for every overlapping pair whose layers match.
class CollisionSystem final : public ISystem {
public:
    explicit CollisionSystem(EventBus& events) noexcept : events_(events) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Collision"; }

private:
    EventBus& events_;
};

/// Applies damage on collision, kills enemies, takes player lives and
/// credits the score.
class DamageSystem final : public ISystem {
public:
    DamageSystem(Registry& registry, EventBus& events, const GameData& data);
    void update(Registry&, float) override {}
    [[nodiscard]] std::string_view name() const override { return "Damage"; }

private:
    void apply(Entity attacker, Entity victim);
    void kill_player(Entity player);
    void credit(std::uint8_t slot, std::uint32_t points);

    Registry& registry_;
    EventBus& events_;
    const GameData& data_;
    engine::events::Subscription on_collision_;
};

class LifetimeSystem final : public ISystem {
public:
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Lifetime"; }
};

/// Respawn delay and invulnerability timers of the players.
class PlayerLifeSystem final : public ISystem {
public:
    PlayerLifeSystem(EntityFactory& factory, EventBus& events) noexcept
        : factory_(factory), events_(events) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "PlayerLife"; }

private:
    EntityFactory& factory_;
    EventBus& events_;
};

/// Wave director: spawns the waves of GameData in order, then loops.
class SpawnSystem final : public ISystem {
public:
    SpawnSystem(EntityFactory& factory, const GameData& data, engine::math::Random& rng) noexcept
        : factory_(factory), data_(data), rng_(rng) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Spawn"; }

    [[nodiscard]] std::size_t cycle() const noexcept { return cycle_; }

private:
    struct PendingSpawn {
        float at_time;
        std::uint8_t enemy;
        float y;
    };

    EntityFactory& factory_;
    const GameData& data_;
    engine::math::Random& rng_;
    float elapsed_ = 0.0F;
    std::size_t next_wave_ = 0;
    std::size_t cycle_ = 0;
    std::vector<PendingSpawn> pending_;
};

/// Advances sprite animations.
class AnimationSystem final : public ISystem {
public:
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Animation"; }
};

/// Destroys entities marked PendingDestroy, announcing each with EntityDestroyed.
class CleanupSystem final : public ISystem {
public:
    explicit CleanupSystem(EventBus& events) noexcept : events_(events) {}
    void update(Registry& registry, float dt) override;
    [[nodiscard]] std::string_view name() const override { return "Cleanup"; }

private:
    EventBus& events_;
};

} // namespace ntype::rtype::systems
