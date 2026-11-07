#include "GameData.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "Loader.hpp"

namespace ntype::server {

namespace {

Wave read_wave(const nlohmann::json& item) {
    Wave wave;
    wave.time = item.at("time").get<float>();
    wave.enemy = item.at("enemy").get<int>();
    wave.count = item.value("count", wave.count);
    wave.interval = item.value("interval", wave.interval);
    wave.x = item.value("x", wave.x);
    wave.y = item.value("y", wave.y);
    wave.y_step = item.value("y_step", wave.y_step);
    return wave;
}

bool load_levels(GameData& data, const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("levels")) {
            Level level;
            level.name = item.value("name", std::string("level"));
            for (const auto& wave : item.at("waves")) {
                level.waves.push_back(read_wave(wave));
            }
            level.boss = read_wave(item.at("boss"));
            level.boss.count = 1;
            data.levels.push_back(std::move(level));
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    // Every enemy a level names must exist.
    for (const Level& level : data.levels) {
        for (const Wave& wave : level.waves) {
            if (data.definitions.EnemyTypes.count(wave.enemy) == 0) {
                error = path + ": level " + level.name + " names unknown enemy " + std::to_string(wave.enemy);
                return false;
            }
        }
        if (data.definitions.EnemyTypes.count(level.boss.enemy) == 0) {
            error = path + ": level " + level.name + " has an unknown boss " + std::to_string(level.boss.enemy);
            return false;
        }
    }
    return true;
}

} // namespace

const GameData& GameData::none() {
    static const GameData empty;
    return empty;
}

bool load_game_data(GameData& data, const std::string& asset_dir, std::string& error) {
    const std::string dir = asset_dir + "/data/";
    World& world = data.definitions;
    // The engine loaders print their own reason to stderr.
    if (!LoadAttackPatterns(world, dir + "attack_patterns.json") ||
        !LoadFlightPatterns(world, dir + "flight_patterns.json") ||
        !LoadEnemies(world, dir + "enemies.json") || !LoadItems(world, dir + "items.json")) {
        error = "cannot load the game data in " + dir;
        return false;
    }
    return load_levels(data, dir + "levels.json", error);
}

} // namespace ntype::server
