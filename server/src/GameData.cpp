#include "GameData.hpp"

#include <fstream>
#include <cmath>
#include <utility>

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
            if (item.contains("boss_support")) {
                const auto& support = item.at("boss_support");
                level.support_enemies = support.at("enemies").get<std::vector<int>>();
                level.support_interval = support.value("interval", 7.0f);
                level.support_max_alive = support.value("max_alive", 2);
            }
            data.levels.push_back(std::move(level));
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    // Every enemy a level names must exist.
    for (const Level& level : data.levels) {
        if (!std::isfinite(level.support_interval) || level.support_interval <= 0 || level.support_max_alive < 1) {
            error = path + ": invalid boss support interval or limit in " + level.name;
            return false;
        }
        for (int enemy : level.support_enemies) {
            const auto type = data.definitions.EnemyTypes.find(enemy);
            if (type == data.definitions.EnemyTypes.end() || type->second.Boss) {
                error = path + ": boss support must name ordinary enemies in " + level.name;
                return false;
            }
        }
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

// "origins" in attack_patterns.json: the points a volley is fired from (a game key).
bool load_origins(GameData& data, const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("attack_patterns")) {
            if (!item.contains("origins")) {
                continue;
            }
            std::vector<sf::Vector2f> points;
            for (const auto& origin : item.at("origins")) {
                points.push_back({origin.at(0).get<float>(), origin.at(1).get<float>()});
            }
            const int id = item.at("id").get<int>();
            if (points.empty()) {
                error = path + ": attack pattern " + std::to_string(id) + ": origins must not be empty";
                return false;
            }
            data.origins[id] = std::move(points);
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    return true;
}

// "beam_pattern" in enemies.json: which enemy types cast which beam (a game key).
bool load_beam_casters(GameData& data, const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("enemies")) {
            if (!item.contains("beam_pattern")) {
                continue;
            }
            const int enemy = item.at("id").get<int>();
            const int beam = item.at("beam_pattern").get<int>();
            if (!data.beams.contains(beam)) {
                error = path + ": enemy " + std::to_string(enemy) + " names " + std::to_string(beam) + ", which is not a beam";
                return false;
            }
            data.beam_casters[enemy] = beam;
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    return true;
}

// The game's own keys of items.json (the engine reads the rest).
bool load_item_rules(GameData& data, const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("items")) {
            ItemRules rules;
            rules.drop_chance = item.value("drop_chance", rules.drop_chance);
            if (item.contains("effects")) {
                const auto& effects = item.at("effects");
                rules.burst_duration = effects.value("burst_duration", rules.burst_duration);
                rules.golden_duration = effects.value("golden_duration", rules.golden_duration);
            }
            data.items[item.at("id").get<int>()] = rules;
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    float total = 0;
    for (const auto& [id, rules] : data.items) {
        if (!std::isfinite(rules.drop_chance) || rules.drop_chance < 0 || rules.drop_chance > 1) {
            error = path + ": invalid drop_chance for item " + std::to_string(id);
            return false;
        }
        total += rules.drop_chance;
    }
    if (total > 1.0f) {
        error = path + ": combined drop chances exceed 1";
        return false;
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
    return load_zone_patterns(dir + "attack_patterns.json", data.zones, error) &&
           load_origins(data, dir + "attack_patterns.json", error) &&
           load_beam_patterns(dir + "attack_patterns.json", data.beams, error) &&
           load_beam_casters(data, dir + "enemies.json", error) &&
           load_item_rules(data, dir + "items.json", error) && load_levels(data, dir + "levels.json", error);
}

} // namespace ntype::server
