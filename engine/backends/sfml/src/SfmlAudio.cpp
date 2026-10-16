#include <algorithm>
#include <exception>
#include <memory>
#include <string>

#include <SFML/Audio/Listener.hpp>

#include <ntype/engine/sfml/SfmlAudio.hpp>
#include <ntype/engine/util/Log.hpp>

namespace ntype::engine::sfml {

namespace {

// Beyond this many simultaneous sounds the oldest finished ones are recycled
// and new sounds are dropped, which keeps a busy fight from saturating OpenAL.
constexpr std::size_t kMaxVoices = 32;

} // namespace

SfmlAudio::SfmlAudio()
    : buffers_([](const std::filesystem::path& path) -> std::unique_ptr<sf::SoundBuffer> {
          auto buffer = std::make_unique<sf::SoundBuffer>();
          if (!buffer->loadFromFile(path)) {
              return nullptr;
          }
          return buffer;
      }) {}

bool SfmlAudio::load_sound(const std::string_view id, const std::filesystem::path& path) {
    try {
        buffers_.load(std::string(id), path);
        return true;
    } catch (const std::exception& error) {
        log::error("{}", error.what());
        return false;
    }
}

void SfmlAudio::play_sound(const std::string_view id, const float volume) {
    const sf::SoundBuffer* buffer = buffers_.find(std::string(id));
    if (buffer == nullptr) {
        return;
    }
    voices_.remove_if(
        [](const sf::Sound& sound) { return sound.getStatus() == sf::Sound::Status::Stopped; });
    if (voices_.size() >= kMaxVoices) {
        return;
    }
    sf::Sound& sound = voices_.emplace_back(*buffer);
    sound.setVolume(std::clamp(volume, 0.0F, 1.0F) * 100.0F);
    sound.play();
}

bool SfmlAudio::play_music(const std::filesystem::path& path) {
    music_.emplace();
    if (!music_->openFromFile(path)) {
        music_.reset();
        log::error("cannot open music {}", path.string());
        return false;
    }
    music_->setLooping(true);
    music_->play();
    return true;
}

void SfmlAudio::stop_music() {
    music_.reset();
}

void SfmlAudio::set_master_volume(const float volume) {
    sf::Listener::setGlobalVolume(std::clamp(volume, 0.0F, 1.0F) * 100.0F);
}

} // namespace ntype::engine::sfml
