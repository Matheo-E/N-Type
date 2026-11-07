#include "Game.hpp"

#include <algorithm>
#include <cmath>

#include "Factory.hpp"
#include "GameConfig.hpp"
#include "System.hpp"

namespace ntype::server {

namespace {

using protocol::InputType;

// Ships are 40x40 boxes; the engine places entities by their centre.
constexpr float kHalf = config::kPlayerSize / 2.0F;
constexpr float kMinCentre = kHalf;
constexpr float kMaxCentreX = static_cast<float>(config::kWidth) - kHalf;
constexpr float kMaxCentreY = static_cast<float>(config::kHeight) - kHalf;

bool pressed(const std::uint8_t keys, const InputType key) {
    return (keys & static_cast<std::uint8_t>(key)) != 0;
}

// Ids 1 to 4 are the players' slots; engine ids map to 5 and up.
std::uint16_t network_id(const int entity) {
    return static_cast<std::uint16_t>(config::kMaxPlayers + 1 + static_cast<unsigned>(entity) % (65536U - 5U));
}

std::uint8_t small(const int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::int32_t rounded(const float value) {
    return static_cast<std::int32_t>(std::lround(value));
}

} // namespace

Game::Game(const GameData& data) : data_(data), world_(data.definitions) {
    if (!data_.levels.empty()) {
        spawned_.assign(data_.levels[level_].waves.size(), 0);
    }
}

void Game::add_ship(const std::uint8_t slot) {
    if (ships_.contains(slot)) {
        return;
    }
    const sf::Vector2f top_left = {100.0F, 150.0F + 120.0F * static_cast<float>(slot - 1)};
    Ship ship;
    ship.entity = CreateEPlayer(world_, top_left + sf::Vector2f(kHalf, kHalf));
    world_.Speeds[ship.entity].Value = config::kPlayerSpeed;
    ships_.emplace(slot, ship);
}

void Game::remove_ship(const std::uint8_t slot) {
    const auto it = ships_.find(slot);
    if (it == ships_.end()) {
        return;
    }
    world_.Destroy(it->second.entity);
    ships_.erase(it);
}

bool Game::has_ship(const std::uint8_t slot) const {
    return ships_.contains(slot);
}

void Game::set_input(const std::uint8_t slot, const std::uint8_t keys) {
    const auto it = ships_.find(slot);
    if (it == ships_.end()) {
        return;
    }
    EPlayer& player = world_.Players[it->second.entity];
    player.Move = {0.0F, 0.0F};
    if (pressed(keys, InputType::Up)) player.Move.y -= 1.0F;
    if (pressed(keys, InputType::Down)) player.Move.y += 1.0F;
    if (pressed(keys, InputType::Left)) player.Move.x -= 1.0F;
    if (pressed(keys, InputType::Right)) player.Move.x += 1.0F;
    player.Shoot = pressed(keys, InputType::Shoot);
}

void Game::step(const float dt) {
    spawn_waves();
    // Same order as the engine's own frame (Action in System.cpp), without drawing.
    MoveInput(world_);
    Spawner(world_, dt); // before Flight, so new bullets start on their flight pattern
    Flight(world_, dt);
    Follow(world_, dt);
    Movement(world_, dt);
    for (const auto& [slot, ship] : ships_) {
        sf::Vector2f& position = world_.Positions[ship.entity].Xy;
        // Players move across a fixed viewport while scenery scrolls independently.
        position.x = std::clamp(position.x, kMinCentre, kMaxCentreX);
        position.y = std::clamp(position.y, kMinCentre, kMaxCentreY);
    }
    Collision(world_);
    Pickup(world_);
    Timers(world_, dt);
    Cull(world_);
    read_deaths();
    level_time_ += dt;
}

void Game::spawn_waves() {
    if (level_ >= data_.levels.size()) {
        return;
    }
    const Level& level = data_.levels[level_];
    for (std::size_t i = 0; i < level.waves.size(); ++i) {
        const Wave& wave = level.waves[i];
        // Every member of the wave whose entry time has come, in order.
        while (spawned_[i] < wave.count &&
               level_time_ >= wave.time + wave.interval * static_cast<float>(spawned_[i])) {
            const float y = wave.y + wave.y_step * static_cast<float>(spawned_[i]);
            CreateEEnemy(world_, {wave.x, y}, wave.enemy);
            ++spawned_[i];
        }
    }
    if (!boss_spawned_ && level_time_ >= level.boss.time) {
        boss_ = CreateEEnemy(world_, {level.boss.x, level.boss.y}, level.boss.enemy);
        boss_spawned_ = true;
    }
}

void Game::read_deaths() {
    for (const EDeath& death : world_.Deaths) {
        if (death.Player) {
            if (const auto slot = slot_of(death.Id)) {
                ships_.erase(*slot); // Collision already destroyed the entity.
                protocol::Entity body{*slot, static_cast<std::uint8_t>(protocol::EntityKind::Player),
                                      rounded(death.Position.x - kHalf), rounded(death.Position.y - kHalf)};
                events_.push_back({GameEvent::Kind::Died, *slot, 0, body});
                events_.push_back({GameEvent::Kind::PlayerDied, *slot, 0, {}});
            }
            continue;
        }
        if (death.EnemyType >= 0) {
            protocol::Entity body{network_id(death.Id), static_cast<std::uint8_t>(protocol::EntityKind::Enemy),
                                  rounded(death.Position.x), rounded(death.Position.y)};
            body.type = small(death.EnemyType);
            events_.push_back({GameEvent::Kind::Died, 0, 0, body});
        }
        if (death.EnemyType >= 0) {
            const auto type = world_.EnemyTypes.find(death.EnemyType);
            const auto slot = slot_of(death.Killer);
            if (slot && type != world_.EnemyTypes.end() && type->second.Score > 0) {
                events_.push_back({GameEvent::Kind::Scored, *slot, static_cast<std::uint32_t>(type->second.Score), {}});
            }
        }
        if (death.Id == boss_) {
            boss_ = -1;
            events_.push_back({GameEvent::Kind::LevelCleared, 0, 0, {}});
        }
    }
    world_.Deaths.clear();
}

std::optional<std::uint8_t> Game::slot_of(const int entity) const {
    for (const auto& [slot, ship] : ships_) {
        if (ship.entity == entity) {
            return slot;
        }
    }
    return std::nullopt;
}

bool Game::next_level() {
    if (level_ + 1 >= data_.levels.size()) {
        return false;
    }
    ++level_;
    level_time_ = 0;
    spawned_.assign(data_.levels[level_].waves.size(), 0);
    boss_spawned_ = false;
    boss_ = -1;
    // A new level starts clean: what is left of the previous one goes.
    std::vector<int> leftovers;
    for (const auto& [id, enemy] : world_.Enemies) leftovers.push_back(id);
    for (const auto& [id, bullet] : world_.Bullets) leftovers.push_back(id);
    for (const auto& [id, pickup] : world_.Pickups) leftovers.push_back(id);
    for (const int id : leftovers) world_.Destroy(id);
    return true;
}

std::vector<GameEvent> Game::take_events() {
    std::vector<GameEvent> events;
    events.swap(events_);
    return events;
}

std::uint8_t Game::health_percent(const int entity) const {
    const auto health = world_.Healths.find(entity);
    if (health == world_.Healths.end() || health->second.MaxHp <= 0) {
        return 0;
    }
    return static_cast<std::uint8_t>(std::clamp(health->second.Hp * 100 / health->second.MaxHp, 0, 100));
}

std::uint8_t Game::hits(const int entity) const {
    const auto health = world_.Healths.find(entity);
    return health == world_.Healths.end() ? 0 : static_cast<std::uint8_t>(health->second.Hits & 0xFF);
}

std::optional<protocol::Entity> Game::describe(const int entity) const {
    using protocol::EntityKind;
    protocol::Entity out;
    if (const auto enemy = world_.Enemies.find(entity); enemy != world_.Enemies.end()) {
        out.kind = static_cast<std::uint8_t>(EntityKind::Enemy);
        out.type = small(enemy->second.TypeId);
        out.hp = health_percent(entity);
        out.hits = hits(entity);
    } else if (const auto bullet = world_.Bullets.find(entity); bullet != world_.Bullets.end()) {
        const auto layer = world_.Layers.find(entity);
        const bool ours = layer != world_.Layers.end() && layer->second.Layer == LayerPlayerBullet;
        out.kind = static_cast<std::uint8_t>(ours ? EntityKind::PlayerShot : EntityKind::EnemyShot);
        out.type = small(bullet->second.PatternId);
    } else if (const auto pickup = world_.Pickups.find(entity); pickup != world_.Pickups.end()) {
        out.kind = static_cast<std::uint8_t>(EntityKind::Item);
        out.type = small(pickup->second.ItemId);
    } else {
        return std::nullopt;
    }
    const auto position = world_.Positions.find(entity);
    if (position == world_.Positions.end()) {
        return std::nullopt;
    }
    out.id = network_id(entity);
    out.x = rounded(position->second.Xy.x);
    out.y = rounded(position->second.Xy.y);
    return out;
}

std::vector<protocol::Entity> Game::snapshot() const {
    std::vector<protocol::Entity> entities;
    for (const auto& [slot, ship] : ships_) {
        const sf::Vector2f& centre = world_.Positions.at(ship.entity).Xy;
        protocol::Entity body{slot, config::kPlayerKind, static_cast<std::int32_t>(centre.x - kHalf),
                              static_cast<std::int32_t>(centre.y - kHalf)};
        body.hp = health_percent(ship.entity);
        body.hits = hits(ship.entity);
        entities.push_back(body);
    }
    // The world's maps are unordered: sort the others by engine id, so a snapshot is
    // stable from one frame to the next.
    std::vector<int> others;
    for (const auto& [id, enemy] : world_.Enemies) others.push_back(id);
    for (const auto& [id, bullet] : world_.Bullets) others.push_back(id);
    for (const auto& [id, pickup] : world_.Pickups) others.push_back(id);
    std::sort(others.begin(), others.end());
    for (const int id : others) {
        if (const auto body = describe(id)) {
            entities.push_back(*body);
        }
    }
    return entities;
}

} // namespace ntype::server
