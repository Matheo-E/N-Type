#pragma once

#include "World.hpp"

#include <string>

// Every loader reads one JSON file and returns false (printing why) if the file is
// missing or malformed, or on the first bad entry. Ids come from the file, and loading
// the same id twice keeps the last one.

// Enemy types -> world.EnemyTypes.
bool LoadEnemies(World& world, const std::string& path);

// Item types -> world.ItemTypes. See ItemData.hpp for the keys; "effects" holds what the
// item does: heal, max_hp, speed, fire_interval, shooting_type, invincibility,
// burst_duration, golden_duration, attack, defense. drop_chance is the probability
// per kill (0..1); the sum for all items must not exceed 1.
bool LoadItems(World& world, const std::string& path);

// Attack patterns -> world.AttackPatterns.
bool LoadAttackPatterns(World& world, const std::string& path);

// Flight patterns -> world.FlightPatterns. "type" must be one of: stationary, straight,
// sine, chase, path. A path also has "steps" (see PathStep in PatternData.hpp).
bool LoadFlightPatterns(World& world, const std::string& path);

// Reads <assetsdir>/data/textures.json and loads every image it lists into
// world.Textures. Image paths are relative to assetsdir. Needs the window to exist
// first: textures live on the graphics card.
bool LoadTextures(World& world, const std::string& assetsdir);

// Call after everything is loaded. Checks that the ids the data refers to exist: the
// patterns and textures named by enemies and items, the shooting type named by items, and
// the textures and flight patterns named by attack patterns.
// Prints every problem and returns false if there was any.
bool CheckReferences(const World& world);
