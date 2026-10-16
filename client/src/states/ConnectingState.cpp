#include "ConnectingState.hpp"

#include <format>
#include <memory>
#include <variant>

#include "PlayState.hpp"

namespace ntype::client {

namespace protocol = rtype::protocol;

namespace {

constexpr float kRetryInterval = 0.5F;

} // namespace

ConnectingState::ConnectingState(ClientContext& context, std::string notice)
    : context_(context), notice_(std::move(notice)), starfield_(context.data.world_size, 180, 7) {}

void ConnectingState::on_enter() {
    server_ = engine::net::Endpoint::resolve(context_.config.host, context_.config.port);
    if (!server_) {
        status_ = std::format("Cannot resolve {}", context_.config.host);
        return;
    }
    context_.network.connect(*server_, engine::time::SteadyClock::now());
    since_request_ = kRetryInterval;
}

void ConnectingState::handle_event(const engine::platform::InputEvent& event) {
    if (const auto* key = std::get_if<engine::platform::KeyPressed>(&event);
        key != nullptr && key->key == engine::platform::Key::Escape) {
        context_.quit();
    }
}

void ConnectingState::update(const float dt) {
    starfield_.update(dt);
    if (!server_) {
        return;
    }
    const auto now = engine::time::SteadyClock::now();
    auto messages = context_.network.poll(now);
    for (std::size_t i = 0; i < messages.size(); ++i) {
        if (const auto* accept = std::get_if<protocol::ConnectAccept>(&messages[i])) {
            // Whatever arrived with the acceptance belongs to the game.
            std::vector<protocol::Message> backlog(
                messages.begin() + static_cast<std::ptrdiff_t>(i) + 1, messages.end());
            context_.states.replace(
                std::make_unique<PlayState>(context_, *accept, std::move(backlog)));
            return;
        }
        if (const auto* refuse = std::get_if<protocol::ConnectRefuse>(&messages[i])) {
            status_ = refuse->reason == protocol::RefuseReason::ServerFull
                          ? "Server full - retrying"
                          : "Server runs another protocol version";
        }
    }
    since_request_ += dt;
    if (since_request_ >= kRetryInterval) {
        since_request_ = 0.0F;
        ++attempts_;
        context_.network.send(protocol::ConnectRequest{}, now);
        if (attempts_ > 1 && status_.empty()) {
            status_ = "No answer yet...";
        }
    }
}

void ConnectingState::render(engine::platform::IRenderer& renderer, float /*alpha*/) {
    const auto size = renderer.logical_size();
    starfield_.render(renderer);
    using engine::platform::TextAlign;
    renderer.draw_text(kFont, "R-TYPE", {size.x / 2.0F, size.y * 0.3F}, 64, {255, 255, 255, 255},
                       TextAlign::Center);
    if (!notice_.empty()) {
        renderer.draw_text(kFont, notice_, {size.x / 2.0F, size.y * 0.5F}, 18, {255, 140, 120, 255},
                           TextAlign::Center);
    }
    const std::string target = std::format("Connecting to {}:{} (attempt {})", context_.config.host,
                                           context_.config.port, attempts_);
    renderer.draw_text(kFont, target, {size.x / 2.0F, size.y * 0.58F}, 16, {200, 200, 255, 255},
                       TextAlign::Center);
    renderer.draw_text(kFont, status_, {size.x / 2.0F, size.y * 0.64F}, 14, {180, 180, 180, 255},
                       TextAlign::Center);
    renderer.draw_text(kFont, "ESC to quit", {size.x / 2.0F, size.y * 0.9F}, 12,
                       {140, 140, 140, 255}, TextAlign::Center);
}

} // namespace ntype::client
