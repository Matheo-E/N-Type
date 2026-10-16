#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include <ntype/rtype/Components.hpp>
#include <ntype/rtype/systems/Systems.hpp>

namespace ntype::rtype::systems {

namespace {

Vec2f normalized(const Vec2f vector) noexcept {
    const float length = vector.length();
    return length > 0.0F ? vector * (1.0F / length) : Vec2f{};
}

bool can_collide(Registry& registry, const Entity entity) {
    if (registry.has<PendingDestroy>(entity)) {
        return false;
    }
    const auto* player = registry.try_get<Player>(entity);
    return player == nullptr || player->alive;
}

} // namespace

void PlayerControlSystem::update(Registry& registry, float /*dt*/) {
    registry.view<PlayerInput, Player, Velocity, Weapon>().each(
        [this](Entity, const PlayerInput& input, const Player& player, Velocity& velocity,
               Weapon& weapon) {
            if (!player.alive) {
                velocity.linear = {};
                weapon.trigger_held = false;
                return;
            }
            Vec2f direction;
            if ((input.bits & input_bits::kUp) != 0) {
                direction.y -= 1.0F;
            }
            if ((input.bits & input_bits::kDown) != 0) {
                direction.y += 1.0F;
            }
            if ((input.bits & input_bits::kLeft) != 0) {
                direction.x -= 1.0F;
            }
            if ((input.bits & input_bits::kRight) != 0) {
                direction.x += 1.0F;
            }
            velocity.linear = normalized(direction) * data_.player.speed;
            weapon.trigger_held = (input.bits & input_bits::kFire) != 0;
        });
}

void EnemyBehaviorSystem::update(Registry& registry, const float dt) {
    registry.view<EnemyBehavior, Velocity>().each(
        [dt](Entity, EnemyBehavior& behavior, Velocity& velocity) {
            behavior.phase += dt;
            velocity.linear.x = -behavior.speed;
            if (behavior.type == BehaviorType::Sine) {
                // Derivative of amplitude * sin(w t): the path stays a sine
                // while the motion goes through the velocity like the rest.
                const float omega = 2.0F * std::numbers::pi_v<float> * behavior.frequency;
                velocity.linear.y = behavior.amplitude * omega * std::cos(omega * behavior.phase);
            }
        });
}

void WeaponSystem::update(Registry& registry, const float dt) {
    std::vector<Vec2f> player_positions;
    registry.view<Player, Transform>().each(
        [&](Entity, const Player& player, const Transform& transform) {
            if (player.alive) {
                player_positions.push_back(transform.position);
            }
        });

    for (const Entity shooter : registry.view<Weapon, Transform>().entities()) {
        auto& weapon = registry.get<Weapon>(shooter);
        weapon.since_last_shot += dt;
        if (!weapon.trigger_held || weapon.since_last_shot < weapon.cooldown ||
            registry.has<PendingDestroy>(shooter)) {
            continue;
        }
        weapon.since_last_shot = 0.0F;
        Vec2f direction{1.0F, 0.0F};
        if (!registry.has<Player>(shooter)) {
            direction = {-1.0F, 0.0F};
            const Vec2f origin = registry.get<Transform>(shooter).position;
            float best = std::numeric_limits<float>::max();
            for (const Vec2f& target : player_positions) {
                const float distance = (target - origin).length();
                if (distance < best && target.x < origin.x) {
                    best = distance;
                    direction = normalized(target - origin);
                }
            }
        }
        factory_.create_missile(shooter, direction);
    }
}

void MovementSystem::update(Registry& registry, const float dt) {
    registry.view<Transform, Velocity>().each(
        [dt](Entity, Transform& transform, const Velocity& velocity) {
            transform.position += velocity.linear * dt;
        });
}

void BoundsSystem::update(Registry& registry, float /*dt*/) {
    constexpr float kMargin = 120.0F;
    registry.view<Transform>().each([&](const Entity entity, Transform& transform) {
        Vec2f& position = transform.position;
        if (registry.has<Player>(entity)) {
            const auto* collider = registry.try_get<Collider>(entity);
            const Vec2f half = collider != nullptr ? collider->size * 0.5F : Vec2f{};
            position.x = std::clamp(position.x, half.x, world_size_.x - half.x);
            position.y = std::clamp(position.y, half.y, world_size_.y - half.y);
            return;
        }
        const bool outside = position.x < -kMargin || position.x > world_size_.x + 2.0F * kMargin ||
                             position.y < -kMargin || position.y > world_size_.y + kMargin;
        if (outside && registry.has<NetworkId>(entity) && !registry.has<PendingDestroy>(entity)) {
            registry.emplace<PendingDestroy>(entity, DestroyReason::Despawned);
        }
    });
}

void CollisionSystem::update(Registry& registry, float /*dt*/) {
    struct Body {
        Entity entity;
        engine::math::Rectf box;
        std::uint8_t layer;
        std::uint8_t mask;
    };
    std::vector<Body> bodies;
    registry.view<Transform, Collider>().each(
        [&](const Entity entity, const Transform& transform, const Collider& collider) {
            if (can_collide(registry, entity)) {
                bodies.push_back({entity,
                                  engine::math::Rectf::centered(transform.position, collider.size),
                                  collider.layer, collider.mask});
            }
        });
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Body& a = bodies[i];
            const Body& b = bodies[j];
            const bool interested = (a.mask & b.layer) != 0 || (b.mask & a.layer) != 0;
            if (interested && a.box.intersects(b.box)) {
                events_.publish(CollisionEvent{a.entity, b.entity});
            }
        }
    }
}

DamageSystem::DamageSystem(Registry& registry, EventBus& events, const GameData& data)
    : registry_(registry), events_(events), data_(data),
      on_collision_(events.subscribe<CollisionEvent>([this](const CollisionEvent& event) {
          apply(event.first, event.second);
          apply(event.second, event.first);
      })) {}

void DamageSystem::apply(const Entity attacker, const Entity victim) {
    if (!registry_.is_alive(attacker) || !registry_.is_alive(victim) ||
        registry_.has<PendingDestroy>(victim)) {
        return;
    }
    // A spent missile cannot hit a second target, but a ship destroyed by
    // this very contact still hurts what it rammed.
    if (registry_.has<Projectile>(attacker) && registry_.has<PendingDestroy>(attacker)) {
        return;
    }
    const auto* damage = registry_.try_get<Damage>(attacker);
    auto* health = registry_.try_get<Health>(victim);
    if (damage == nullptr || health == nullptr) {
        return;
    }
    if (const auto* player = registry_.try_get<Player>(victim);
        player != nullptr && (!player->alive || player->invulnerable > 0.0F)) {
        return;
    }
    health->current -= damage->amount;
    const auto* projectile = registry_.try_get<Projectile>(attacker);
    if (projectile != nullptr) {
        registry_.emplace<PendingDestroy>(attacker, DestroyReason::Hit);
    }
    if (health->current > 0) {
        return;
    }
    if (registry_.has<Player>(victim)) {
        kill_player(victim);
        return;
    }
    registry_.emplace<PendingDestroy>(victim, DestroyReason::Killed);
    if (const auto* behavior = registry_.try_get<EnemyBehavior>(victim)) {
        std::uint8_t killer = Projectile::kEnemyOwner;
        if (projectile != nullptr) {
            killer = projectile->owner_slot;
        } else if (const auto* player = registry_.try_get<Player>(attacker)) {
            killer = player->slot;
        }
        credit(killer, behavior->score_value);
        events_.publish(EnemyKilled{victim, killer, behavior->score_value});
    }
}

void DamageSystem::kill_player(const Entity player) {
    auto& state = registry_.get<Player>(player);
    state.alive = false;
    state.lives = state.lives > 0 ? static_cast<std::uint8_t>(state.lives - 1) : 0;
    state.respawn_timer = data_.player.respawn_delay;
    registry_.get<Velocity>(player).linear = {};
    events_.publish(PlayerDied{state.slot, state.lives});
}

void DamageSystem::credit(const std::uint8_t slot, const std::uint32_t points) {
    if (slot == Projectile::kEnemyOwner) {
        return;
    }
    registry_.view<Player, Score>().each([&](Entity, const Player& player, Score& score) {
        if (player.slot == slot) {
            score.points += points;
        }
    });
}

void LifetimeSystem::update(Registry& registry, const float dt) {
    registry.view<Lifetime>().each([&](const Entity entity, Lifetime& lifetime) {
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0.0F && !registry.has<PendingDestroy>(entity)) {
            registry.emplace<PendingDestroy>(entity, DestroyReason::Expired);
        }
    });
}

void PlayerLifeSystem::update(Registry& registry, const float dt) {
    for (const Entity entity : registry.view<Player>().entities()) {
        auto& player = registry.get<Player>(entity);
        if (player.alive) {
            player.invulnerable = std::max(0.0F, player.invulnerable - dt);
            continue;
        }
        if (player.lives == 0) {
            continue;
        }
        player.respawn_timer -= dt;
        if (player.respawn_timer <= 0.0F) {
            factory_.respawn_player(entity);
            events_.publish(PlayerRespawned{player.slot});
        }
    }
}

void SpawnSystem::update(Registry& /*registry*/, const float dt) {
    const auto& waves = data_.waves;
    if (waves.empty()) {
        return;
    }
    elapsed_ += dt;
    while (next_wave_ < waves.size() && waves[next_wave_].at_time <= elapsed_) {
        const WaveDef& wave = waves[next_wave_];
        const float y =
            wave.y >= 0.0F ? wave.y : rng_.next_float(80.0F, data_.world_size.y - 80.0F);
        for (std::uint8_t i = 0; i < wave.count; ++i) {
            pending_.push_back(
                {wave.at_time + wave.spacing * static_cast<float>(i), wave.enemy, y});
        }
        ++next_wave_;
    }
    const float spawn_x = data_.world_size.x + 40.0F;
    std::erase_if(pending_, [&](const PendingSpawn& spawn) {
        if (spawn.at_time > elapsed_) {
            return false;
        }
        factory_.create_enemy(spawn.enemy, {spawn_x, spawn.y}, rng_);
        return true;
    });
    if (next_wave_ == waves.size() && pending_.empty() &&
        elapsed_ >= waves.back().at_time + data_.wave_loop_delay) {
        elapsed_ = 0.0F;
        next_wave_ = 0;
        ++cycle_;
    }
}

void AnimationSystem::update(Registry& registry, const float dt) {
    registry.view<Animation, Sprite>().each([dt](Entity, Animation& animation, Sprite& sprite) {
        if (animation.frames.empty() || animation.frame_time <= 0.0F) {
            return;
        }
        animation.elapsed += dt;
        while (animation.elapsed >= animation.frame_time) {
            animation.elapsed -= animation.frame_time;
            if (animation.current + 1 < animation.frames.size()) {
                ++animation.current;
            } else if (animation.loop) {
                animation.current = 0;
            }
        }
        sprite.source = animation.frames[animation.current];
    });
}

void CleanupSystem::update(Registry& registry, float /*dt*/) {
    for (const Entity entity : registry.view<PendingDestroy>().entities()) {
        const auto reason = registry.get<PendingDestroy>(entity).reason;
        const auto* network_id = registry.try_get<NetworkId>(entity);
        events_.publish(
            EntityDestroyed{entity, network_id != nullptr ? network_id->id : 0, reason});
        registry.destroy(entity);
    }
}

} // namespace ntype::rtype::systems
