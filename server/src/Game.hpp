#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "GameData.hpp"
#include "Protocol.hpp"
#include "World.hpp"

namespace ntype::server {

// Something the lobby must hear about, from one step of the game.
struct GameEvent {
    enum class Kind {
        Scored,       // `slot` killed an enemy worth `points`
        PlayerDied,   // `slot` lost its ship
        LevelCleared, // the level's boss is dead
        Died,         // `body` died (an enemy or a player): clients show the explosion
    };
    Kind kind = Kind::Scored;
    std::uint8_t slot = 0;
    std::uint32_t points = 0;
    protocol::Entity body;
};

// One running game: one engine World with one ship per lobby slot (1 to 4), the
// enemies of the current level's waves, and every bullet. The server is
// authoritative: ships only move and shoot from the keys their player sent.
class Game {
public:
    explicit Game(const GameData& data = GameData::none());

    // Spawns the ship of `slot` with full health; does nothing if it already exists.
    void add_ship(std::uint8_t slot);
    void remove_ship(std::uint8_t slot);
    bool has_ship(std::uint8_t slot) const;
    // Ignored when `slot` has no ship.
    void set_input(std::uint8_t slot, std::uint8_t keys);
    void step(float dt);
    // Every entity: the ships first, ordered by slot (id = slot, position = top-left
    // corner of the 40x40 box), then enemies, shots and items (id from 5, position =
    // centre), ordered by id.
    std::vector<protocol::Entity> snapshot() const;

    // What happened since the last call, oldest first.
    std::vector<GameEvent> take_events();

    // Zero-based index of the level being played.
    std::size_t level() const { return level_; }
    // Starts the next level: enemies and bullets go, its waves start over.
    // False (and nothing changes) after the last level.
    bool next_level();

    // The engine world, for tests and tools.
    World& world() { return world_; }

private:
    struct Ship {
        int entity = 0;
    };

    void spawn_waves();
    void read_deaths();
    void drop_item(sf::Vector2f position);
    std::optional<std::uint8_t> slot_of(int entity) const;
    // How an enemy, shot or item is sent; nothing for anything else.
    std::optional<protocol::Entity> describe(int entity) const;
    std::uint8_t health_percent(int entity) const;
    std::uint8_t hits(int entity) const;

    const GameData& data_;
    World world_;
    std::map<std::uint8_t, Ship> ships_;
    std::size_t level_ = 0;
    float level_time_ = 0;
    std::vector<int> spawned_; // per wave of the level: how many entered so far
    bool boss_spawned_ = false;
    int boss_ = -1;
    float next_support_time_ = 0;
    std::size_t support_spawned_ = 0;
    std::mt19937 drop_random_{std::random_device{}()};
    std::vector<GameEvent> events_;
};

} // namespace ntype::server
