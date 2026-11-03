#pragma once

#include "World.hpp"

#include <string>

// Loads an image once and returns its texture id. Asking again for the same path
// returns the same id. Returns -1 (and prints why) if the file cannot be loaded.
int LoadTexture(World& world, const std::string& path);

// Reads <assetsdir>/data/enemies.json into world.EnemyTypes, loading the textures
// it names (paths in the file are relative to assetsdir).
// Returns false (and prints why) if the file is missing or malformed.
bool LoadEnemies(World& world, const std::string& assetsdir);
