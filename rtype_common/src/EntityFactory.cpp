#include <stdexcept>

#include <ntype/rtype/Components.hpp>
#include <ntype/rtype/EntityFactory.hpp>

namespace ntype::rtype {

namespace {

constexpr int kLayerProjectiles = 1;
constexpr int kLayerEnemies = 2;
constexpr int kLayerPlayers = 3;
constexpr int kLayerEffects = 4;

} // namespace

void EntityFactory::add_visual(const Entity entity, const SpriteDef& sprite, const int layer) {
    registry_.emplace<Sprite>(entity, Sprite{sprite.texture, sprite.frames.front(), sprite.scale,
                                             layer, engine::platform::colors::kWhite});
    if (sprite.frames.size() > 1) {
        registry_.emplace<Animation>(entity, Animation{sprite.frames, sprite.frame_time});
    }
}

Vec2f EntityFactory::player_start(const std::uint8_t slot) const noexcept {
    const float lane = data_.world_size.y / 5.0F;
    return {data_.world_size.x * 0.12F, lane * static_cast<float>(slot + 1)};
}

Entity EntityFactory::create_player(const std::uint8_t slot) {
    return create_player_entity(slot, next_network_id_++);
}

Entity EntityFactory::create_player_entity(const std::uint8_t slot,
                                           const std::uint32_t network_id) {
    const PlayerDef& def = data_.player;
    const Entity entity = registry_.create();
    registry_.emplace<Transform>(entity, player_start(slot));
    registry_.emplace<Velocity>(entity);
    registry_.emplace<Collider>(
        entity, def.size, collision_layers::kPlayer,
        static_cast<std::uint8_t>(collision_layers::kEnemy | collision_layers::kEnemyShot));
    registry_.emplace<Health>(entity, def.hp, def.hp);
    registry_.emplace<Damage>(entity, 1);
    registry_.emplace<Weapon>(entity, def.fire_cooldown, def.fire_cooldown, def.projectile, false);
    registry_.emplace<NetworkId>(entity, network_id);
    registry_.emplace<EntityType>(entity, EntityKind::Player, slot);
    registry_.emplace<PlayerInput>(entity);
    registry_.emplace<Player>(entity, slot, def.lives, true, 0.0F, def.invulnerability);
    registry_.emplace<Score>(entity);
    add_visual(entity, def.sprites.at(slot % def.sprites.size()), kLayerPlayers);
    return entity;
}

void EntityFactory::respawn_player(const Entity player) {
    auto& state = registry_.get<Player>(player);
    registry_.get<Transform>(player).position = player_start(state.slot);
    registry_.get<Velocity>(player).linear = {};
    auto& health = registry_.get<Health>(player);
    health.current = health.max;
    state.alive = true;
    state.invulnerable = data_.player.invulnerability;
}

Entity EntityFactory::create_enemy(const std::uint8_t enemy_type, const Vec2f position,
                                   engine::math::Random& rng) {
    const Entity entity = create_enemy_entity(enemy_type, position, next_network_id_++);
    const EnemyDef& def = data_.enemies.at(enemy_type);
    if (def.shoots()) {
        auto& weapon = registry_.emplace<Weapon>(entity);
        weapon.cooldown = rng.next_float(def.fire_cooldown_min, def.fire_cooldown_max);
        weapon.since_last_shot = rng.next_float(0.0F, weapon.cooldown);
        weapon.projectile = def.projectile;
        weapon.trigger_held = true;
    }
    return entity;
}

Entity EntityFactory::create_enemy_entity(const std::uint8_t enemy_type, const Vec2f position,
                                          const std::uint32_t network_id) {
    const EnemyDef* def = data_.find_enemy(enemy_type);
    if (def == nullptr) {
        throw std::out_of_range("unknown enemy type");
    }
    const Entity entity = registry_.create();
    registry_.emplace<Transform>(entity, position);
    registry_.emplace<Velocity>(entity, Vec2f{-def->speed, 0.0F});
    registry_.emplace<Collider>(
        entity, def->size, collision_layers::kEnemy,
        static_cast<std::uint8_t>(collision_layers::kPlayer | collision_layers::kPlayerShot));
    registry_.emplace<Health>(entity, def->hp, def->hp);
    registry_.emplace<Damage>(entity, def->contact_damage);
    registry_.emplace<NetworkId>(entity, network_id);
    registry_.emplace<EntityType>(entity, EntityKind::Enemy, enemy_type);
    registry_.emplace<EnemyBehavior>(entity, def->behavior, def->speed, def->amplitude,
                                     def->frequency, 0.0F, position, def->score);
    add_visual(entity, def->sprite, kLayerEnemies);
    return entity;
}

Entity EntityFactory::create_missile(const Entity owner, const Vec2f direction) {
    const auto& weapon = registry_.get<Weapon>(owner);
    const Vec2f origin = registry_.get<Transform>(owner).position;
    const auto* player = registry_.try_get<Player>(owner);
    const EntityKind kind =
        player != nullptr ? EntityKind::PlayerMissile : EntityKind::EnemyMissile;
    const std::uint8_t owner_slot = player != nullptr ? player->slot : Projectile::kEnemyOwner;
    const auto* collider = registry_.try_get<Collider>(owner);
    const float muzzle = collider != nullptr ? collider->size.x / 2.0F : 0.0F;
    return create_missile_entity(kind, weapon.projectile, origin + direction * muzzle, direction,
                                 owner_slot, next_network_id_++);
}

Entity EntityFactory::create_missile_entity(const EntityKind kind, const std::uint8_t projectile,
                                            const Vec2f position, const Vec2f direction,
                                            const std::uint8_t owner_slot,
                                            const std::uint32_t network_id) {
    const ProjectileDef* def = data_.find_projectile(projectile);
    if (def == nullptr) {
        throw std::out_of_range("unknown projectile type");
    }
    const bool from_player = kind == EntityKind::PlayerMissile;
    const Entity entity = registry_.create();
    registry_.emplace<Transform>(entity, position);
    registry_.emplace<Velocity>(entity, direction * def->speed);
    registry_.emplace<Collider>(entity, def->size,
                                from_player ? collision_layers::kPlayerShot
                                            : collision_layers::kEnemyShot,
                                from_player ? collision_layers::kEnemy : collision_layers::kPlayer);
    registry_.emplace<Damage>(entity, def->damage);
    registry_.emplace<Lifetime>(entity, def->lifetime);
    registry_.emplace<Projectile>(entity, owner_slot);
    registry_.emplace<NetworkId>(entity, network_id);
    registry_.emplace<EntityType>(entity, kind, projectile);
    add_visual(entity, def->sprite, kLayerProjectiles);
    return entity;
}

Entity EntityFactory::create_from_spawn(const protocol::SpawnedEntity& spawned) {
    const Vec2f position{protocol::dequantize_position(spawned.x),
                         protocol::dequantize_position(spawned.y)};
    switch (spawned.kind) {
    case EntityKind::Player: {
        const Entity player = create_player_entity(
            static_cast<std::uint8_t>(spawned.subtype % protocol::kMaxPlayers), spawned.network_id);
        registry_.get<Transform>(player).position = position;
        return player;
    }
    case EntityKind::Enemy:
        return create_enemy_entity(spawned.subtype, position, spawned.network_id);
    case EntityKind::PlayerMissile:
    case EntityKind::EnemyMissile: {
        const Vec2f direction{spawned.kind == EntityKind::PlayerMissile ? 1.0F : -1.0F, 0.0F};
        return create_missile_entity(spawned.kind, spawned.subtype, position, direction,
                                     Projectile::kEnemyOwner, spawned.network_id);
    }
    }
    throw std::out_of_range("unknown entity kind");
}

Entity EntityFactory::create_explosion(const Vec2f position) {
    const Entity entity = registry_.create();
    registry_.emplace<Transform>(entity, position);
    const SpriteDef& sprite = data_.explosion;
    add_visual(entity, sprite, kLayerEffects);
    if (auto* animation = registry_.try_get<Animation>(entity)) {
        animation->loop = false;
    }
    registry_.emplace<Lifetime>(entity,
                                sprite.frame_time * static_cast<float>(sprite.frames.size()));
    return entity;
}

} // namespace ntype::rtype
