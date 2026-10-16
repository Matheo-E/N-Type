#include "ClientApp.hpp"

#include <memory>

#include <ntype/engine/util/Log.hpp>

#include "states/ConnectingState.hpp"

namespace ntype::client {

ClientApp::ClientApp(ClientConfig config, rtype::GameData data, std::filesystem::path assets)
    : config_(std::move(config)), data_(std::move(data)), assets_(std::move(assets)),
      window_({1280, 720}, "R-Type"), renderer_(window_, data_.world_size), input_(window_),
      loop_(engine::time::Duration(1.0 / 60.0)),
      context_{config_, data_,    renderer_, audio_,
               input_,  network_, states_,   [this] { loop_.stop(); }} {}

void ClientApp::load_assets() {
    for (const auto& texture : data_.textures) {
        renderer_.load_texture(texture.id, assets_ / texture.path);
    }
    for (const auto& sound : data_.sounds) {
        audio_.load_sound(sound.id, assets_ / sound.path);
    }
    if (!data_.font.empty() && !renderer_.load_font(kFont, assets_ / data_.font)) {
        engine::log::warn("no font: texts will not be displayed");
    }
}

void ClientApp::fixed_update(const float dt) {
    for (const auto& event : window_.poll_events()) {
        if (std::holds_alternative<engine::platform::WindowClosed>(event)) {
            network_.disconnect(engine::time::SteadyClock::now());
            loop_.stop();
            return;
        }
        states_.handle_event(event);
    }
    states_.update(dt);
}

void ClientApp::render(const float alpha) {
    renderer_.begin_frame({4, 4, 16, 255});
    states_.render(renderer_, alpha);
    constexpr double kScreenshotDelay = 6.0;
    if (!config_.screenshot.empty() && uptime_.elapsed().count() > kScreenshotDelay) {
        if (renderer_.save_screenshot(config_.screenshot)) {
            engine::log::info("screenshot saved to {}", config_.screenshot.string());
        }
        config_.screenshot.clear();
        network_.disconnect(engine::time::SteadyClock::now());
        loop_.stop();
    }
    renderer_.end_frame();
}

int ClientApp::run() {
    load_assets();
    states_.push(std::make_unique<ConnectingState>(context_));
    loop_.run([this](const float dt) { fixed_update(dt); },
              [this](const float alpha) { render(alpha); });
    window_.close();
    return 0;
}

} // namespace ntype::client
