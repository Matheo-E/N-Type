#include "ImpactSounds.hpp"

#include <stdexcept>
#include <string>

#include "UiClickSound.hpp"

namespace ntype::client {

namespace {

constexpr float kEnemyGapSeconds = 0.05f; // at most 20 ticks per second

void load(sf::SoundBuffer& buffer, const std::string& file) {
    const std::string path = std::string(NTYPE_ASSET_DIR) + "/audio/sfx/" + file;
    if (!buffer.loadFromFile(path)) throw std::runtime_error("Unable to load impact sound: " + path);
}

} // namespace

ImpactSounds::ImpactSounds() {
    load(enemy_buffer_, "hit_enemy.wav");
    load(player_buffer_, "hit_player.wav");
    for (auto& voice : enemy_voices_) {
        voice.setBuffer(enemy_buffer_);
        voice.setVolume(35.f);
    }
    for (auto& voice : player_voices_) {
        voice.setBuffer(player_buffer_);
        voice.setVolume(70.f);
    }
}

void ImpactSounds::play(std::array<sf::Sound, 6>& voices, std::size_t& next) {
    if (!UiClickSound::instance().enabled()) return; // the pause menu's sound effects toggle
    auto& voice = voices[next];
    next = (next + 1U) % voices.size();
    voice.stop();
    voice.play();
}

void ImpactSounds::enemy_hit() {
    if (since_enemy_.getElapsedTime().asSeconds() < kEnemyGapSeconds) return;
    since_enemy_.restart();
    play(enemy_voices_, next_enemy_);
}

void ImpactSounds::player_hit() {
    play(player_voices_, next_player_);
}

} // namespace ntype::client
