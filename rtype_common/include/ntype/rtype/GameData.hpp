#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ntype/rtype/Components.hpp>

namespace ntype::rtype {

/// How to draw one kind of entity.
struct SpriteDef {
    std::string texture;
    std::vector<Recti> frames;
    float frame_time = 0.1F;
    float scale = 1.0F;
};

struct PlayerDef {
    float speed = 300.0F;
    std::int32_t hp = 1;
    std::uint8_t lives = 3;
    float fire_cooldown = 0.2F;
    std::uint8_t projectile = 0;
    float respawn_delay = 2.0F;
    float invulnerability = 2.0F;
    Vec2f size;
    /// One sprite per player slot, so the four players look different.
    std::vector<SpriteDef> sprites;
};

struct ProjectileDef {
    std::string name;
    float speed = 600.0F;
    std::int32_t damage = 1;
    float lifetime = 3.0F;
    Vec2f size;
    SpriteDef sprite;
};

struct EnemyDef {
    std::string name;
    std::int32_t hp = 1;
    float speed = 100.0F;
    BehaviorType behavior = BehaviorType::Straight;
    float amplitude = 0.0F;
    float frequency = 0.0F;
    std::uint32_t score = 100;
    std::int32_t contact_damage = 1;
    float fire_cooldown_min = 0.0F;
    float fire_cooldown_max = 0.0F;
    std::uint8_t projectile = 0;
    Vec2f size;
    SpriteDef sprite;

    [[nodiscard]] bool shoots() const noexcept { return fire_cooldown_max > 0.0F; }
};

struct WaveDef {
    float at_time = 0.0F;
    std::uint8_t enemy = 0;
    std::uint8_t count = 1;
    float spacing = 0.5F;
    /// Vertical spawn position; negative means random.
    float y = -1.0F;
};

struct TextureDef {
    std::string id;
    std::string path;
};

struct SoundDef {
    std::string id;
    std::string path;
};

/// Everything data-driven about the game, loaded from assets/data/game.json.
/// Adding an enemy or a wave only means editing that file.
struct GameData {
    Vec2f world_size{1280.0F, 720.0F};
    PlayerDef player;
    std::vector<ProjectileDef> projectiles;
    std::vector<EnemyDef> enemies;
    std::vector<WaveDef> waves;
    float wave_loop_delay = 3.0F;
    SpriteDef explosion;
    std::vector<TextureDef> textures;
    std::vector<SoundDef> sounds;
    std::string font;

    /// Throws std::runtime_error with a readable message on invalid data.
    [[nodiscard]] static GameData load_from_file(const std::filesystem::path& path);
    [[nodiscard]] static GameData load_from_string(std::string_view json);

    [[nodiscard]] const EnemyDef* find_enemy(std::uint8_t id) const noexcept;
    [[nodiscard]] const ProjectileDef* find_projectile(std::uint8_t id) const noexcept;
};

} // namespace ntype::rtype
