#pragma once

#include <string>
#include <vector>

#include "World.hpp"

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
};

// Everything a game is built from, loaded once by the server and shared by every lobby.
struct GameData {
    World definitions; // enemy types, patterns and items; no entities
    std::vector<Level> levels;

    // No enemies and no levels: ships only (used when there is nothing to load, and by tests).
    static const GameData& none();
};

// Reads assets/data: enemies, attack and flight patterns, items and levels.
// Returns false and fills `error` if a file is missing or wrong.
bool load_game_data(GameData& data, const std::string& asset_dir, std::string& error);

} // namespace ntype::server
