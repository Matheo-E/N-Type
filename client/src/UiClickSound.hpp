#pragma once

#include <stdexcept>
#include <string>

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

namespace ntype::client {

/**
 * @brief Owns the shared sound effect used by every clickable UI button.
 *
 * The singleton outlives launcher and game screen transitions, so actions such
 * as restarting or returning to the lobby do not cut the short sound early.
 */
class UiClickSound {
public:
    /** @brief Returns the single click sound player used by the client. */
    static UiClickSound& instance() {
        static UiClickSound sound;
        return sound;
    }

    /** @brief Restarts the click effect to keep rapid clicks responsive. */
    void play() {
        sound_.stop();
        sound_.play();
    }

private:
    UiClickSound() {
        const std::string path =
            std::string(NTYPE_ASSET_DIR) + "/audio/sfx/button_click.mp3";
        if (!buffer_.loadFromFile(path))
            throw std::runtime_error("Unable to load UI click sound: " + path);
        sound_.setBuffer(buffer_);
        sound_.setVolume(55.f);
    }

    sf::SoundBuffer buffer_;
    sf::Sound sound_;
};

} // namespace ntype::client
