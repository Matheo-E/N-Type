#pragma once

#include "World.hpp"

#include <string>

// Reads the enemy types from a JSON file into world.EnemyTypes.
// Returns false (and prints why) if the file is missing or malformed.
bool LoadEnemies(World& world, const std::string& path);

// Reads <assetsdir>/data/textures.json and loads every image it lists into
// world.Textures, under the id written in the file. Image paths are relative to
// assetsdir. Needs the window to exist first: textures live on the graphics card.
// Returns false (and prints why) on the first problem.
bool LoadTextures(World& world, const std::string& assetsdir);
