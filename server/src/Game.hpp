#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <utility>
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

    // Seconds of burst fire and of the golden look left for `slot` (0 without a ship).
    float burst_time(std::uint8_t slot) const;
    float golden_time(std::uint8_t slot) const;
    // Target zones on the ground.
    std::size_t zone_count() const { return zones_.size(); }

private:
    struct Ship {
        int entity = 0;
        // Power-ups from items (ItemRules in GameData.hpp).
        float burst_time = 0;      // seconds of three-volley bursts left
        float burst_timer = 0.6F;  // seconds since the last volley of a burst
        int burst_remaining = 0;   // volleys left in the current burst
        float golden_time = 0;     // seconds of the golden look left
    };
    // A target zone on the ground (ZonePatterns.hpp): a stationary enemy shot that the game,
    // not the engine's Collision, lets hurt.
    struct Zone {
        int owner = -1;
        float warning = 0;          // seconds before it hurts
        float initial_warning = 0;
        float radius = 0;
        std::map<int, float> cooldowns; // per target: seconds before it can hurt it again
    };
    // A beam across the screen (BeamPatterns.hpp): one entity, not a bullet; the game decides
    // where it hurts.
    struct Beam {
        int owner = -1;
        int pattern = -1;
        float warning = 0;  // seconds before it hurts
        std::set<int> hit;  // players it already hurt: once each
    };
    // A ship under burst fire, while the engine's spawner runs (see prepare_bursts).
    struct BurstHold {
        int entity = 0;
        bool shoot = false; // the player's real fire key
        float timer = 0;    // the spawner timer the engine would have kept
    };
    // Shots the engine's spawner is about to fire this frame that the game handles itself.
    struct Volleys {
        int first_new = 0;                                // first entity id created by the spawner
        std::vector<std::pair<int, int>> muted;           // shooter, pattern: the game fires these
        std::vector<std::pair<int, sf::Vector2f>> zones;  // caster, where its zone goes
    };

    void spawn_waves();
    // The game's own rules, around the engine's systems in step().
    std::vector<BurstHold> prepare_bursts(float dt);
    void finish_bursts(const std::vector<BurstHold>& holds);
    Volleys prepare_volleys(float dt);
    void finish_volleys(const Volleys& volleys);
    void fire_from_origins(int shooter, int pattern);
    void make_zone(int shot, int caster, sf::Vector2f where);
    void fire_beams(float dt);
    void beam_hits();
    void keep_casters_reachable(float dt);
    void zone_hits();
    std::vector<std::pair<int, int>> pickups_about_to_happen();
    void apply_pickups(const std::vector<std::pair<int, int>>& picked);
    void count_down(float dt);
    void clear_zones();
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
    std::map<int, Zone> zones_; // by entity
    std::map<int, Beam> beams_;          // by entity
    std::map<int, float> beam_timers_;   // by caster: seconds since its last beam
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
