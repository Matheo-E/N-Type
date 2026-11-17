#pragma once

#include <array>
#include <cstddef>

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/System/Clock.hpp>

namespace ntype::client {

/**
 * @brief The two impact sounds: a bright tick when an enemy is hit, a deeper thud when
 * a player is (assets/audio/sfx/hit_enemy.wav and hit_player.wav).
 *
 * Several voices let close hits overlap. Enemy ticks are spaced a little, so four
 * players firing at once do not turn them into a buzz. Both follow the pause menu's
 * sound effects toggle.
 */
class ImpactSounds {
public:
    ImpactSounds();
    void enemy_hit();
    void player_hit();

private:
    static void play(std::array<sf::Sound, 6>& voices, std::size_t& next);

    sf::SoundBuffer enemy_buffer_;
    sf::SoundBuffer player_buffer_;
    std::array<sf::Sound, 6> enemy_voices_;
    std::array<sf::Sound, 6> player_voices_;
    std::size_t next_enemy_ = 0;
    std::size_t next_player_ = 0;
    sf::Clock since_enemy_;
};

} // namespace ntype::client
