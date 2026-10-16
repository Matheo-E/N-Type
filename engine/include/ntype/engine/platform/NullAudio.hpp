#pragma once

#include <ntype/engine/platform/IAudio.hpp>

namespace ntype::engine::platform {

/// Audio device that plays nothing (headless builds, tests, no sound card).
class NullAudio final : public IAudio {
public:
    bool load_sound(std::string_view, const std::filesystem::path&) override { return true; }
    void play_sound(std::string_view, float) override {}
    bool play_music(const std::filesystem::path&) override { return true; }
    void stop_music() override {}
    void set_master_volume(float) override {}
};

} // namespace ntype::engine::platform
