#include "../include/Loader.hpp"

#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// "sprite": { "texture": <id in textures.json>, "rect": [x, y, w, h], "scale": n }
void ReadSprite(const nlohmann::json& sprite, EnemyData& data) {
    data.TextureId = sprite.value("texture", data.TextureId);
    data.Scale = sprite.value("scale", data.Scale);
    const std::vector<int> rect = sprite.value("rect", std::vector<int>{});
    if (rect.size() >= 4) {
        data.Source = sf::IntRect(rect[0], rect[1], rect[2], rect[3]);
    }
}

// Keys other than "id" are optional: a missing one keeps the default.
EnemyData ReadEnemy(const nlohmann::json& item) {
    EnemyData data;
    data.Name = item.value("name", data.Name);
    data.Hp = item.value("hp", data.Hp);
    data.Radius = item.value("radius", data.Radius);
    data.Speed = item.value("speed", data.Speed);
    data.FireInterval = item.value("fire_interval", data.FireInterval);
    data.AttackPatternId = item.value("attack_pattern", data.AttackPatternId);
    data.FlightPatternId = item.value("flight_pattern", data.FlightPatternId);

    if (item.contains("sprite")) {
        ReadSprite(item.at("sprite"), data);
    }
    return data;
}

// Loads one entry of textures.json into world.Textures. Throws on a missing or
// mistyped key; returns false if the image itself cannot be loaded.
bool ReadTexture(World& world, const std::string& assetsdir, const nlohmann::json& item) {
    const int id = item.at("id").get<int>();
    const std::string path = item.at("path").get<std::string>();

    AllTexture& entry = world.Textures[id];
    entry.Path = assetsdir + "/" + path;
    if (!entry.Image.loadFromFile(entry.Path)) {
        std::cerr << "cannot load texture " << entry.Path << "\n";
        world.Textures.erase(id);
        return false;
    }
    return true;
}

}  // namespace

bool LoadEnemies(World& world, const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "cannot open " << path << "\n";
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("enemies")) {
            world.EnemyTypes[item.at("id").get<int>()] = ReadEnemy(item);
        }
    } catch (const nlohmann::json::exception& error) {
        std::cerr << path << ": " << error.what() << "\n";
        return false;
    }
    return true;
}

bool LoadTextures(World& world, const std::string& assetsdir) {
    const std::string path = assetsdir + "/data/textures.json";
    std::ifstream file(path);
    if (!file) {
        std::cerr << "cannot open " << path << "\n";
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("textures")) {
            if (!ReadTexture(world, assetsdir, item)) {
                return false;
            }
        }
    } catch (const nlohmann::json::exception& error) {
        std::cerr << path << ": " << error.what() << "\n";
        return false;
    }
    return true;
}
