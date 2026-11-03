#include "../include/Loader.hpp"

#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// Keys other than "id" are optional: a missing one keeps the default.
void ReadSprite(World& world, const std::string& assetsdir, const nlohmann::json& sprite, EnemyData& data) {
    const std::string file = sprite.value("texture", "");
    if (file.empty()) {
        return;
    }
    data.TextureId = LoadTexture(world, assetsdir + "/" + file);
    data.Scale = sprite.value("scale", data.Scale);
    const std::vector<int> rect = sprite.value("rect", std::vector<int>{});
    if (rect.size() >= 4) {
        data.Source = sf::IntRect(rect[0], rect[1], rect[2], rect[3]);
    }
}

EnemyData ReadEnemy(World& world, const std::string& assetsdir, const nlohmann::json& item) {
    EnemyData data;
    data.Name = item.value("name", data.Name);
    data.Hp = item.value("hp", data.Hp);
    data.Radius = item.value("radius", data.Radius);
    data.Speed = item.value("speed", data.Speed);
    data.FireInterval = item.value("fire_interval", data.FireInterval);
    data.AttackPatternId = item.value("attack_pattern", data.AttackPatternId);
    data.FlightPatternId = item.value("flight_pattern", data.FlightPatternId);

    const std::vector<int> color = item.value("color", std::vector<int>{});
    if (color.size() >= 3) {
        data.Color = sf::Color(color[0], color[1], color[2]);
    }
    if (item.contains("sprite")) {
        ReadSprite(world, assetsdir, item.at("sprite"), data);
    }
    return data;
}

}  // namespace

int LoadTexture(World& world, const std::string& path) {
    auto known = world.TexturePaths.find(path);
    if (known != world.TexturePaths.end()) {
        return known->second;
    }
    sf::Texture texture;
    if (!texture.loadFromFile(path)) {
        std::cerr << "cannot load texture " << path << "\n";
        return -1;
    }
    int id = world.NextTextureId++;
    world.Textures[id] = texture;
    world.TexturePaths[path] = id;
    return id;
}

bool LoadEnemies(World& world, const std::string& assetsdir) {
    const std::string path = assetsdir + "/data/enemies.json";
    std::ifstream file(path);
    if (!file) {
        std::cerr << "cannot open " << path << "\n";
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("enemies")) {
            world.EnemyTypes[item.at("id").get<int>()] = ReadEnemy(world, assetsdir, item);
        }
    } catch (const nlohmann::json::exception& error) {
        std::cerr << path << ": " << error.what() << "\n";
        return false;
    }
    return true;
}
