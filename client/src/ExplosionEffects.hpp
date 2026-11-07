#pragma once

#include <array>
#include <vector>

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

namespace ntype::client {

/**
 * @brief Plays the enemy death explosion: a 180-frame animation and its sound.
 *
 * The animation is split across two 10x9 sheets of 256x144 frames:
 * `enemy_die_spritesheet.png` (frames 0-89) continues into
 * `enemy_die_spritesheet2.png` (frames 90-179). Both were keyed from the
 * green-screen source video, frames 6-185.
 * Any number of explosions may run at once; each is removed after its last frame.
 */
class ExplosionEffects {
public:
    ExplosionEffects();

    /**
     * @brief Starts an explosion centered on a world position and plays its sound.
     * @param center Point in game coordinates, typically the dead enemy's center.
     * @param scale Display scale applied to the 256x144 frames.
     */
    void spawn(sf::Vector2f center, float scale = 1.f);
    /** @brief Advances every running explosion and drops finished ones. */
    void update(float elapsed_seconds);
    void draw(sf::RenderTarget& target) const;

private:
    struct Explosion {
        sf::Vector2f center;
        float scale = 1.f;
        float elapsed_seconds = 0.f;
    };

    std::array<sf::Texture, 2> sheets_;
    sf::SoundBuffer sound_buffer_;
    // Several voices so overlapping deaths do not cut each other's sound.
    std::array<sf::Sound, 8> voices_;
    std::size_t next_voice_ = 0;
    std::vector<Explosion> explosions_;
};

} // namespace ntype::client
