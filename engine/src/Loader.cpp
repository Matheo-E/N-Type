#include "../include/Loader.hpp"

#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

// Opens and parses a JSON file, then calls each(item) for every element of the array
// under "key". each returns false to stop. Any JSON problem (bad syntax, a missing or
// mistyped key) is reported here.
template <class Each>
bool ReadList(const std::string& path, const char* key, Each each) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "cannot open " << path << "\n";
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at(key)) {
            if (!each(item)) {
                return false;
            }
        }
    } catch (const nlohmann::json::exception& error) {
        std::cerr << path << ": " << error.what() << "\n";
        return false;
    }
    return true;
}

// "sprite": { "texture": <id in textures.json>, "rect": [x, y, w, h], "scale": n, "flip_x": true }
void ReadSprite(const nlohmann::json& sprite, int& texture, sf::IntRect& source, float& scale) {
    texture = sprite.value("texture", texture);
    scale = sprite.value("scale", scale);
    const std::vector<int> rect = sprite.value("rect", std::vector<int>{});
    if (rect.size() >= 4) {
        source = sf::IntRect(rect[0], rect[1], rect[2], rect[3]);
    }
}

// A full sprite block, shared by enemies and items:
// { "texture": 2, "rect": [x, y, w, h], "scale": 1, "flip_x": true,
//   "animation": { "frames": [0, 1, 2, 1], "frame_time": 0.125 } }
template <class Data>
void ReadLook(const nlohmann::json& sprite, Data& data) {
    ReadSprite(sprite, data.TextureId, data.Source, data.Scale);
    data.FlipX = sprite.value("flip_x", data.FlipX);
    if (sprite.contains("animation")) {
        const nlohmann::json& animation = sprite.at("animation");
        data.AnimationFrames = animation.at("frames").get<std::vector<int>>();
        data.AnimationFrameTime = animation.value("frame_time", data.AnimationFrameTime);
    }
}

// Keys other than "id" are optional: a missing one keeps the default.
EnemyData ReadEnemy(const nlohmann::json& item) {
    EnemyData data;
    data.Name = item.value("name", data.Name);
    data.Hp = item.value("hp", data.Hp);
    data.ContactDamage = item.value("contact_damage", data.ContactDamage);
    data.Score = item.value("score", data.Score);
    data.Boss = item.value("boss", data.Boss);
    data.Radius = item.value("radius", data.Radius);
    data.Speed = item.value("speed", data.Speed);
    data.FireInterval = item.value("fire_interval", data.FireInterval);
    data.AttackPatternId = item.value("attack_pattern", data.AttackPatternId);
    data.FlightPatternId = item.value("flight_pattern", data.FlightPatternId);
    if (item.contains("sprite")) {
        ReadLook(item.at("sprite"), data);
    }
    return data;
}

// Keys other than "id" are optional. "effects" lists what the item does when picked up.
ItemData ReadItem(const nlohmann::json& item) {
    ItemData data;
    data.Name = item.value("name", data.Name);
    data.Radius = item.value("radius", data.Radius);
    data.FlightPatternId = item.value("flight_pattern", data.FlightPatternId);
    data.Speed = item.value("speed", data.Speed);
    data.Lifetime = item.value("lifetime", data.Lifetime);
    if (item.contains("sprite")) {
        ReadLook(item.at("sprite"), data);
    }
    if (item.contains("effects")) {
        const nlohmann::json& effects = item.at("effects");
        data.Heal = effects.value("heal", data.Heal);
        data.MaxHp = effects.value("max_hp", data.MaxHp);
        data.SpeedBonus = effects.value("speed", data.SpeedBonus);
        data.FireInterval = effects.value("fire_interval", data.FireInterval);
        data.ShootingType = effects.value("shooting_type", data.ShootingType);
        data.Invincibility = effects.value("invincibility", data.Invincibility);
        data.Attack = effects.value("attack", data.Attack);
        data.Defense = effects.value("defense", data.Defense);
    }
    return data;
}

AttackPattern ReadAttack(const nlohmann::json& item) {
    AttackPattern pattern;
    pattern.Name = item.value("name", pattern.Name);
    pattern.Count = item.value("count", pattern.Count);
    pattern.SpreadDegrees = item.value("spread", pattern.SpreadDegrees);
    pattern.AngleDegrees = item.value("angle", pattern.AngleDegrees);
    pattern.Aimed = item.value("aimed", pattern.Aimed);
    pattern.Speed = item.value("speed", pattern.Speed);
    pattern.Damage = item.value("damage", pattern.Damage);
    pattern.Radius = item.value("radius", pattern.Radius);
    pattern.Lifetime = item.value("lifetime", pattern.Lifetime);
    pattern.FlightPatternId = item.value("flight_pattern", pattern.FlightPatternId);
    if (item.contains("sprite")) {
        ReadLook(item.at("sprite"), pattern);
    }
    return pattern;
}

bool ParseFlightType(const std::string& name, FlightType& type) {
    if (name == "stationary") { type = FlightType::Stationary; return true; }
    if (name == "straight") { type = FlightType::Straight; return true; }
    if (name == "sine") { type = FlightType::Sine; return true; }
    if (name == "chase") { type = FlightType::Chase; return true; }
    if (name == "path") { type = FlightType::Path; return true; }
    return false;
}

sf::Vector2f ReadPoint(const nlohmann::json& point) {
    return {point.at(0).get<float>(), point.at(1).get<float>()};
}

// One step of a path: "duration" plus exactly one of "move", "curve", "arc", "wait".
//   { "move": [x, y], "duration": 1.5 }
//   { "curve": [[c1x, c1y], [c2x, c2y], [x, y]], "duration": 2 }
//   { "arc": { "radius": 80, "start": 180, "sweep": 360 }, "duration": 3 }
//   { "wait": true, "duration": 1 }
bool ReadStep(const nlohmann::json& item, int patternid, PathStep& step) {
    step.Duration = item.at("duration").get<float>();
    const int kinds = int(item.contains("move")) + int(item.contains("curve")) + int(item.contains("arc")) + int(item.contains("wait"));
    if (kinds != 1 || step.Duration <= 0.0f) {
        std::cerr << "flight pattern " << patternid << ": each step needs a duration above 0 and exactly one of move, curve, arc, wait\n";
        return false;
    }
    if (item.contains("move")) {
        step.Kind = StepKind::Move;
        step.To = ReadPoint(item.at("move"));
    } else if (item.contains("curve")) {
        const nlohmann::json& points = item.at("curve");
        if (points.size() != 3) {
            std::cerr << "flight pattern " << patternid << ": a curve needs 3 points (two controls and the end)\n";
            return false;
        }
        step.Kind = StepKind::Curve;
        step.Control1 = ReadPoint(points.at(0));
        step.Control2 = ReadPoint(points.at(1));
        step.To = ReadPoint(points.at(2));
    } else if (item.contains("arc")) {
        const nlohmann::json& arc = item.at("arc");
        step.Kind = StepKind::Arc;
        step.Radius = arc.at("radius").get<float>();
        step.StartDegrees = arc.value("start", 0.0f);
        step.SweepDegrees = arc.at("sweep").get<float>();
    } else {
        step.Kind = StepKind::Wait;
    }
    return true;
}

// "type" is required, so that a typo is an error and not a silently motionless enemy.
bool ReadFlight(const nlohmann::json& item, FlightPattern& pattern) {
    const std::string type = item.at("type").get<std::string>();
    if (!ParseFlightType(type, pattern.Type)) {
        std::cerr << "flight pattern " << item.at("id") << ": unknown type \"" << type << "\"\n";
        return false;
    }
    pattern.Name = item.value("name", pattern.Name);
    pattern.DirectionDegrees = item.value("direction", pattern.DirectionDegrees);
    pattern.Forward = item.value("forward", pattern.Forward);
    pattern.Amplitude = item.value("amplitude", pattern.Amplitude);
    pattern.Frequency = item.value("frequency", pattern.Frequency);
    if (pattern.Type == FlightType::Path) {
        const nlohmann::json& steps = item.at("steps");
        if (steps.empty()) {
            std::cerr << "flight pattern " << item.at("id") << ": a path needs at least one step\n";
            return false;
        }
        for (const auto& step : steps) {
            pattern.Steps.emplace_back();
            if (!ReadStep(step, item.at("id").get<int>(), pattern.Steps.back())) {
                return false;
            }
        }
    }
    return true;
}

// Loads one entry of textures.json into world.Textures. Returns false if the image
// itself cannot be loaded.
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
    return ReadList(path, "enemies", [&world](const nlohmann::json& item) {
        world.EnemyTypes[item.at("id").get<int>()] = ReadEnemy(item);
        return true;
    });
}

bool LoadItems(World& world, const std::string& path) {
    return ReadList(path, "items", [&world](const nlohmann::json& item) {
        world.ItemTypes[item.at("id").get<int>()] = ReadItem(item);
        return true;
    });
}

bool LoadAttackPatterns(World& world, const std::string& path) {
    return ReadList(path, "attack_patterns", [&world](const nlohmann::json& item) {
        world.AttackPatterns[item.at("id").get<int>()] = ReadAttack(item);
        return true;
    });
}

bool LoadFlightPatterns(World& world, const std::string& path) {
    return ReadList(path, "flight_patterns", [&world](const nlohmann::json& item) {
        FlightPattern pattern;
        if (!ReadFlight(item, pattern)) {
            return false;
        }
        world.FlightPatterns[item.at("id").get<int>()] = pattern;
        return true;
    });
}

bool LoadTextures(World& world, const std::string& assetsdir) {
    return ReadList(assetsdir + "/data/textures.json", "textures",
                    [&world, &assetsdir](const nlohmann::json& item) {
                        return ReadTexture(world, assetsdir, item);
                    });
}

bool CheckReferences(const World& world) {
    bool ok = true;
    auto missing = [&ok](const std::string& who, const char* what, int id) {
        std::cerr << who << ": unknown " << what << " " << id << "\n";
        ok = false;
    };
    for (const auto& [id, enemy] : world.EnemyTypes) {
        const std::string who = "enemy " + std::to_string(id) + " (" + enemy.Name + ")";
        if (enemy.AttackPatternId >= 0 && world.AttackPatterns.count(enemy.AttackPatternId) == 0) {
            missing(who, "attack pattern", enemy.AttackPatternId);
        }
        if (enemy.FlightPatternId >= 0 && world.FlightPatterns.count(enemy.FlightPatternId) == 0) {
            missing(who, "flight pattern", enemy.FlightPatternId);
        }
        if (enemy.TextureId >= 0 && world.Textures.count(enemy.TextureId) == 0) {
            missing(who, "texture", enemy.TextureId);
        }
    }
    for (const auto& [id, data] : world.ItemTypes) {
        const std::string who = "item " + std::to_string(id) + " (" + data.Name + ")";
        if (data.TextureId >= 0 && world.Textures.count(data.TextureId) == 0) {
            missing(who, "texture", data.TextureId);
        }
        if (data.FlightPatternId >= 0 && world.FlightPatterns.count(data.FlightPatternId) == 0) {
            missing(who, "flight pattern", data.FlightPatternId);
        }
        if (data.ShootingType >= 0 && world.AttackPatterns.count(data.ShootingType) == 0) {
            missing(who, "attack pattern (shooting_type)", data.ShootingType);
        }
    }
    for (const auto& [id, pattern] : world.AttackPatterns) {
        const std::string who = "attack pattern " + std::to_string(id) + " (" + pattern.Name + ")";
        if (pattern.TextureId >= 0 && world.Textures.count(pattern.TextureId) == 0) {
            missing(who, "texture", pattern.TextureId);
        }
        if (pattern.FlightPatternId >= 0 && world.FlightPatterns.count(pattern.FlightPatternId) == 0) {
            missing(who, "flight pattern", pattern.FlightPatternId);
        }
    }
    return ok;
}
