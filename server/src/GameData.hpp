#pragma once

#include <map>
#include <string>
#include <vector>

#include "World.hpp"
#include "BeamPatterns.hpp"
#include "ZonePatterns.hpp"

namespace ntype::server {

// A group of enemies of one type, entering one after the other (assets/data/levels.json).
struct Wave {
    float time = 0;     // seconds after the level starts
    int enemy = -1;     // id in enemies.json
    int count = 1;
    float interval = 1; // seconds between two of them
    float x = 1320;     // where they appear: just right of the 1280-wide play area
    float y = 360;
    float y_step = 0;   // added to y for each next one
};

struct Level {
    std::string name;
    std::vector<Wave> waves;
    Wave boss; // count 1; killing it clears the level
    std::vector<int> support_enemies; // optional repeating reinforcements while the boss lives
    float support_interval = 7;
    int support_max_alive = 2;
};

// What items do in this game beyond their engine effects (keys of assets/data/items.json).
struct ItemRules {
    float drop_chance = 0;     // "drop_chance": probability per non-boss kill; all items together <= 1
    float burst_duration = 0;  // "effects.burst_duration": seconds of three-volley bursts
    float golden_duration = 0; // "effects.golden_duration": seconds shown with the golden sprite
};

// Everything a game is built from, loaded once by the server and shared by every lobby.
struct GameData {
    World definitions; // enemy types, patterns and items; no entities
    std::vector<Level> levels;
    std::map<int, ZonePattern> zones;  // attack patterns that are target zones, by id
    // Attack patterns fired from several points ("origins": [[x, y], ...], relative to the
    // shooter), by id. The game fires these itself, one volley from each point.
    std::map<int, std::vector<sf::Vector2f>> origins;
    std::map<int, BeamPattern> beams;  // attack patterns that are beams, by id
    std::map<int, int> beam_casters;   // enemy type -> its beam pattern ("beam_pattern" in enemies.json)
    std::map<int, ItemRules> items;    // by item id
    bool invincible = false; // Temporary testing mode; all player ships are protected

    // No enemies and no levels: ships only (used when there is nothing to load, and by tests).
    static const GameData& none();
};

// Reads assets/data: enemies, attack and flight patterns, items and levels.
// Returns false and fills `error` if a file is missing or wrong.
bool load_game_data(GameData& data, const std::string& asset_dir, std::string& error);

} // namespace ntype::server
