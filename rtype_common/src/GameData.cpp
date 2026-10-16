#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include <ntype/rtype/GameData.hpp>

namespace ntype::rtype {

namespace {

using nlohmann::json;

Vec2f read_vec2(const json& value) {
    return {value.at(0).get<float>(), value.at(1).get<float>()};
}

SpriteDef read_sprite(const json& value) {
    SpriteDef sprite;
    sprite.texture = value.at("texture").get<std::string>();
    for (const auto& frame : value.at("frames")) {
        sprite.frames.push_back({frame.at(0).get<int>(), frame.at(1).get<int>(),
                                 frame.at(2).get<int>(), frame.at(3).get<int>()});
    }
    if (sprite.frames.empty()) {
        throw std::runtime_error("sprite '" + sprite.texture + "' has no frames");
    }
    sprite.frame_time = value.value("frame_time", 0.1F);
    sprite.scale = value.value("scale", 1.0F);
    return sprite;
}

BehaviorType read_behavior(const std::string& name) {
    if (name == "straight") {
        return BehaviorType::Straight;
    }
    if (name == "sine") {
        return BehaviorType::Sine;
    }
    throw std::runtime_error("unknown enemy behavior '" + name + "'");
}

template <typename Def>
std::uint8_t index_of(const std::vector<Def>& defs, const std::string& name) {
    for (std::size_t i = 0; i < defs.size(); ++i) {
        if (defs[i].name == name) {
            return static_cast<std::uint8_t>(i);
        }
    }
    throw std::runtime_error("unknown reference '" + name + "'");
}

GameData parse(const json& root) {
    GameData data;
    data.world_size = read_vec2(root.at("world_size"));

    for (const auto& texture : root.at("textures").items()) {
        data.textures.push_back({texture.key(), texture.value().get<std::string>()});
    }
    // Named, not iterated as a temporary: before C++23 the temporary returned
    // by value() would be destroyed before the loop body runs.
    const json sounds = root.value("sounds", json::object());
    for (const auto& sound : sounds.items()) {
        data.sounds.push_back({sound.key(), sound.value().get<std::string>()});
    }
    data.font = root.value("font", std::string{});

    for (const auto& value : root.at("projectiles")) {
        ProjectileDef projectile;
        projectile.name = value.at("name").get<std::string>();
        projectile.speed = value.at("speed").get<float>();
        projectile.damage = value.value("damage", 1);
        projectile.lifetime = value.value("lifetime", 3.0F);
        projectile.size = read_vec2(value.at("size"));
        projectile.sprite = read_sprite(value.at("sprite"));
        data.projectiles.push_back(std::move(projectile));
    }

    const json& player = root.at("player");
    data.player.speed = player.at("speed").get<float>();
    data.player.hp = player.value("hp", 1);
    data.player.lives = player.value<std::uint8_t>("lives", 3);
    data.player.fire_cooldown = player.at("fire_cooldown").get<float>();
    data.player.projectile = index_of(data.projectiles, player.at("projectile").get<std::string>());
    data.player.respawn_delay = player.value("respawn_delay", 2.0F);
    data.player.invulnerability = player.value("invulnerability", 2.0F);
    data.player.size = read_vec2(player.at("size"));
    for (const auto& sprite : player.at("sprites")) {
        data.player.sprites.push_back(read_sprite(sprite));
    }
    if (data.player.sprites.size() < 4) {
        throw std::runtime_error("the player needs one sprite per slot (4)");
    }

    for (const auto& value : root.at("enemies")) {
        EnemyDef enemy;
        enemy.name = value.at("name").get<std::string>();
        enemy.hp = value.at("hp").get<std::int32_t>();
        enemy.speed = value.at("speed").get<float>();
        enemy.behavior = read_behavior(value.value("behavior", std::string{"straight"}));
        enemy.amplitude = value.value("amplitude", 0.0F);
        enemy.frequency = value.value("frequency", 0.0F);
        enemy.score = value.value("score", 100U);
        enemy.contact_damage = value.value("contact_damage", 1);
        if (const auto fire = value.find("fire_cooldown"); fire != value.end()) {
            enemy.fire_cooldown_min = fire->at(0).get<float>();
            enemy.fire_cooldown_max = fire->at(1).get<float>();
            enemy.projectile =
                index_of(data.projectiles, value.at("projectile").get<std::string>());
        }
        enemy.size = read_vec2(value.at("size"));
        enemy.sprite = read_sprite(value.at("sprite"));
        data.enemies.push_back(std::move(enemy));
    }
    if (data.enemies.empty() || data.enemies.size() > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("there must be between 1 and 255 enemy types");
    }

    for (const auto& value : root.at("waves")) {
        WaveDef wave;
        wave.at_time = value.at("at").get<float>();
        wave.enemy = index_of(data.enemies, value.at("enemy").get<std::string>());
        wave.count = value.value<std::uint8_t>("count", 1);
        wave.spacing = value.value("spacing", 0.5F);
        wave.y = value.value("y", -1.0F);
        data.waves.push_back(wave);
    }
    data.wave_loop_delay = root.value("wave_loop_delay", 3.0F);
    data.explosion = read_sprite(root.at("explosion"));
    return data;
}

} // namespace

GameData GameData::load_from_string(const std::string_view text) {
    try {
        return parse(json::parse(text));
    } catch (const json::exception& error) {
        throw std::runtime_error(std::string("invalid game data: ") + error.what());
    }
}

GameData GameData::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open game data " + path.string());
    }
    std::ostringstream content;
    content << file.rdbuf();
    return load_from_string(content.str());
}

const EnemyDef* GameData::find_enemy(const std::uint8_t id) const noexcept {
    return id < enemies.size() ? &enemies[id] : nullptr;
}

const ProjectileDef* GameData::find_projectile(const std::uint8_t id) const noexcept {
    return id < projectiles.size() ? &projectiles[id] : nullptr;
}

} // namespace ntype::rtype
