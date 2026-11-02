#include "Game.hpp"

#include <algorithm>

#include "Factory.hpp"
#include "GameConfig.hpp"
#include "System.hpp"

namespace ntype::server {

namespace {

using protocol::InputType;

constexpr float kMaxX = static_cast<float>(config::kWidth) - config::kPlayerSize;
constexpr float kMaxY = static_cast<float>(config::kHeight) - config::kPlayerSize;

bool pressed(const std::uint8_t keys, const InputType key) {
    return (keys & static_cast<std::uint8_t>(key)) != 0;
}

} // namespace

void Game::add_ship(const std::uint8_t slot) {
    if (ships_.contains(slot)) {
        return;
    }
    Ship ship;
    ship.entity = Create_E(world_);
    world_.Positions[ship.entity].Xy = {100.0F, 150.0F + 120.0F * static_cast<float>(slot - 1)};
    world_.Velocities[ship.entity];
    ships_.emplace(slot, ship);
}

void Game::remove_ship(const std::uint8_t slot) {
    const auto it = ships_.find(slot);
    if (it == ships_.end()) {
        return;
    }
    world_.Positions.erase(it->second.entity);
    world_.Velocities.erase(it->second.entity);
    ships_.erase(it);
}

bool Game::has_ship(const std::uint8_t slot) const {
    return ships_.contains(slot);
}

void Game::set_input(const std::uint8_t slot, const std::uint8_t keys) {
    if (const auto it = ships_.find(slot); it != ships_.end()) {
        it->second.keys = keys;
    }
}

void Game::step(const float dt) {
    for (const auto& [slot, ship] : ships_) {
        float dx = 0.0F;
        float dy = 0.0F;
        if (pressed(ship.keys, InputType::Up)) {
            dy -= 1.0F;
        }
        if (pressed(ship.keys, InputType::Down)) {
            dy += 1.0F;
        }
        if (pressed(ship.keys, InputType::Left)) {
            dx -= 1.0F;
        }
        if (pressed(ship.keys, InputType::Right)) {
            dx += 1.0F;
        }
        world_.Velocities[ship.entity].Xy = {dx * config::kPlayerSpeed, dy * config::kPlayerSpeed};
    }
    Movement(world_, dt);
    for (const auto& [slot, ship] : ships_) {
        sf::Vector2f& position = world_.Positions[ship.entity].Xy;
        position.x = std::clamp(position.x, 0.0F, kMaxX);
        position.y = std::clamp(position.y, 0.0F, kMaxY);
    }
}

std::vector<protocol::Entity> Game::snapshot() const {
    std::vector<protocol::Entity> entities;
    for (const auto& [slot, ship] : ships_) {
        const sf::Vector2f& position = world_.Positions.at(ship.entity).Xy;
        entities.push_back({slot, config::kPlayerKind, static_cast<std::int16_t>(position.x),
                            static_cast<std::int16_t>(position.y)});
    }
    return entities;
}

} // namespace ntype::server
