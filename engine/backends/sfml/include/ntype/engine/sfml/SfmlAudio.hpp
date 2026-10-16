#pragma once

#include <list>
#include <optional>

#include <SFML/Audio/Music.hpp>
#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include <ntype/engine/platform/IAudio.hpp>
#include <ntype/engine/resources/ResourceManager.hpp>

namespace ntype::engine::sfml {

class SfmlAudio final : public platform::IAudio {
public:
    SfmlAudio();

    bool load_sound(std::string_view id, const std::filesystem::path& path) override;
    void play_sound(std::string_view id, float volume) override;
    bool play_music(const std::filesystem::path& path) override;
    void stop_music() override;
    void set_master_volume(float volume) override;

private:
    resources::ResourceManager<sf::SoundBuffer> buffers_;
    // A list keeps sounds at a stable address while they play.
    std::list<sf::Sound> voices_;
    std::optional<sf::Music> music_;
};

} // namespace ntype::engine::sfml
