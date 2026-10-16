#pragma once

#include <filesystem>
#include <string_view>

namespace ntype::engine::platform {

class IAudio {
public:
    virtual ~IAudio() = default;

    /// Returns false when the file cannot be loaded.
    virtual bool load_sound(std::string_view id, const std::filesystem::path& path) = 0;
    /// `volume` in [0, 1], multiplied by the master volume.
    virtual void play_sound(std::string_view id, float volume) = 0;
    virtual bool play_music(const std::filesystem::path& path) = 0;
    virtual void stop_music() = 0;
    /// `volume` in [0, 1].
    virtual void set_master_volume(float volume) = 0;
};

} // namespace ntype::engine::platform
