#include "ExplosionEffects.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <SFML/Graphics/Sprite.hpp>

#include "UiClickSound.hpp"

namespace ntype::client {

namespace {

constexpr int kFrameWidth = 256;
constexpr int kFrameHeight = 144;
constexpr int kSheetColumns = 10;
constexpr int kFramesPerSheet = kSheetColumns * 9;
constexpr int kTotalFrames = kFramesPerSheet * 2;
// The source video runs at 30 fps; playing it twice as fast lasts 3 s,
// matching the length of the sound.
constexpr float kFramesPerSecond = 60.f;
// Center of the visible cloud inside a frame; the art sits low in each cell.
constexpr float kOriginX = 125.f;
constexpr float kOriginY = 95.f;
constexpr float kSoundVolume = 60.f;

} // namespace

ExplosionEffects::ExplosionEffects() {
    const std::string dir = std::string(NTYPE_ASSET_DIR);
    const std::array<std::string, 2> sheet_paths{
        dir + "/sprites/effects/enemy_die_spritesheet.png",
        dir + "/sprites/effects/enemy_die_spritesheet2.png",
    };
    for (std::size_t i = 0; i < sheets_.size(); ++i) {
        if (!sheets_[i].loadFromFile(sheet_paths[i]))
            throw std::runtime_error("Unable to load explosion sheet: " + sheet_paths[i]);
        sheets_[i].setSmooth(true);
    }
    const std::string sound_path = dir + "/audio/sfx/enemy-die.mp3";
    if (!sound_buffer_.loadFromFile(sound_path))
        throw std::runtime_error("Unable to load explosion sound: " + sound_path);
    for (auto& voice : voices_) {
        voice.setBuffer(sound_buffer_);
        voice.setVolume(kSoundVolume);
    }
}

void ExplosionEffects::spawn(const sf::Vector2f center, const float scale) {
    explosions_.push_back({center, scale, 0.f});
    // Shares the pause menu's sound effects toggle.
    if (!UiClickSound::instance().enabled()) return;
    auto& voice = voices_[next_voice_];
    next_voice_ = (next_voice_ + 1U) % voices_.size();
    voice.stop();
    voice.play();
}

void ExplosionEffects::update(const float elapsed_seconds) {
    for (auto& explosion : explosions_) explosion.elapsed_seconds += elapsed_seconds;
    const float duration = static_cast<float>(kTotalFrames) / kFramesPerSecond;
    explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(),
                                     [duration](const Explosion& explosion) {
                                         return explosion.elapsed_seconds >= duration;
                                     }),
                      explosions_.end());
}

void ExplosionEffects::draw(sf::RenderTarget& target) const {
    for (const auto& explosion : explosions_) {
        const int frame = std::min(
            static_cast<int>(explosion.elapsed_seconds * kFramesPerSecond), kTotalFrames - 1);
        const int cell = frame % kFramesPerSheet;
        sf::Sprite sprite(sheets_[static_cast<std::size_t>(frame / kFramesPerSheet)]);
        sprite.setTextureRect({(cell % kSheetColumns) * kFrameWidth,
                               (cell / kSheetColumns) * kFrameHeight, kFrameWidth,
                               kFrameHeight});
        sprite.setOrigin(kOriginX, kOriginY);
        sprite.setScale(explosion.scale, explosion.scale);
        sprite.setPosition(explosion.center);
        target.draw(sprite);
    }
}

} // namespace ntype::client
