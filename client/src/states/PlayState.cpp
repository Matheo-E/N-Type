#include "PlayState.hpp"

#include <algorithm>
#include <format>
#include <memory>

#include "../ClientEvents.hpp"
#include "ConnectingState.hpp"

namespace ntype::client {

namespace protocol = rtype::protocol;
using engine::platform::Color;
using engine::platform::TextAlign;

namespace {

// One colour per slot, matching the ship sprites (cyan, pink, green, red).
constexpr std::array<Color, 4> kPlayerColors{{
    {90, 220, 255, 255},
    {255, 120, 220, 255},
    {120, 255, 120, 255},
    {255, 120, 100, 255},
}};

float snapshot_period(const protocol::ConnectAccept& accept) {
    // The server sends a snapshot roughly every 1/30 s whatever its tick rate.
    const unsigned interval = std::max(1U, accept.tick_rate / 30U);
    return static_cast<float>(interval) / static_cast<float>(accept.tick_rate);
}

} // namespace

PlayState::PlayState(ClientContext& context, const protocol::ConnectAccept& accept,
                     std::vector<protocol::Message> backlog)
    : context_(context), accept_(accept), backlog_(std::move(backlog)),
      factory_(registry_, context.data),
      sync_(registry_, factory_, events_, snapshot_period(accept)),
      starfield_(context.data.world_size, 180, accept.seed), cleanup_(events_),
      render_system_(context.renderer) {
    subscriptions_.push_back(events_.subscribe<ReplicaSpawned>([this](const ReplicaSpawned& event) {
        if (event.kind == rtype::EntityKind::PlayerMissile) {
            context_.audio.play_sound("shot", 0.35F);
        }
    }));
    subscriptions_.push_back(
        events_.subscribe<ReplicaDestroyed>([this](const ReplicaDestroyed& event) {
            if (event.reason == rtype::DestroyReason::Killed) {
                factory_.create_explosion(event.position);
                context_.audio.play_sound("explosion", 0.6F);
            }
        }));
    show_notice(std::format("You are player {}", accept.slot + 1));
}

void PlayState::show_notice(std::string text) {
    notice_ = std::move(text);
    notice_timer_ = 3.0F;
}

void PlayState::handle_event(const engine::platform::InputEvent& event) {
    if (const auto* key = std::get_if<engine::platform::KeyPressed>(&event);
        key != nullptr && key->key == engine::platform::Key::Escape) {
        context_.network.disconnect(engine::time::SteadyClock::now());
        context_.quit();
    }
}

void PlayState::handle(const protocol::Message& message) {
    std::visit(
        [this](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, protocol::EntitySpawn> ||
                          std::is_same_v<T, protocol::EntityDestroy>) {
                sync_.apply(concrete);
            } else if constexpr (std::is_same_v<T, protocol::WorldSnapshot>) {
                sync_.apply(concrete);
                if (!concrete.players.empty()) {
                    players_ = concrete.players;
                }
            } else if constexpr (std::is_same_v<T, protocol::GameStateChange>) {
                game_state_ = concrete.state;
            } else if constexpr (std::is_same_v<T, protocol::PlayerEvent>) {
                if (concrete.type == protocol::PlayerEventType::Died) {
                    context_.audio.play_sound("player_hit", 0.8F);
                    if (concrete.slot == accept_.slot) {
                        show_notice(concrete.value > 0
                                        ? std::format("Ship lost - {} left", concrete.value)
                                        : std::string("No ships left"));
                    }
                }
            } else if constexpr (std::is_same_v<T, protocol::ClientLeft>) {
                show_notice(std::format("Player {} {}", concrete.slot + 1,
                                        concrete.reason == protocol::LeaveReason::TimedOut
                                            ? "lost connection"
                                            : "left the game"));
            }
        },
        message);
}

void PlayState::update(const float dt) {
    const auto now = engine::time::SteadyClock::now();
    for (const auto& message : std::exchange(backlog_, {})) {
        handle(message);
    }
    for (const auto& message : context_.network.poll(now)) {
        handle(message);
    }
    if (context_.network.timed_out(now)) {
        context_.states.replace(std::make_unique<ConnectingState>(context_, "Connection lost"));
        return;
    }

    context_.network.send(protocol::Input{++client_tick_, input_mapper_.sample(context_.input)},
                          now);
    context_.network.update(now);

    lifetime_.update(registry_, dt);
    cleanup_.update(registry_, dt);
    notice_timer_ = std::max(0.0F, notice_timer_ - dt);
}

void PlayState::render(engine::platform::IRenderer& renderer, float /*alpha*/) {
    // Visual-only motion runs at the display rate for smoothness.
    const auto dt = static_cast<float>(std::min(frame_clock_.restart().count(), 0.1));
    time_ += dt;
    starfield_.update(dt);
    interpolation_.update(registry_, dt);
    animation_.update(registry_, dt);

    starfield_.render(renderer);
    render_system_.draw(registry_, time_);
    draw_hud(renderer);
}

void PlayState::draw_hud(engine::platform::IRenderer& renderer) const {
    const auto size = renderer.logical_size();
    float x = 20.0F;
    for (const auto& player : players_) {
        const Color color = kPlayerColors.at(player.slot % kPlayerColors.size());
        const std::string marker = player.slot == accept_.slot ? ">" : " ";
        renderer.draw_text(
            kFont,
            std::format("{}P{} {:06} x{}", marker, player.slot + 1, player.score, player.lives),
            {x, 16.0F}, 14, color, TextAlign::Left);
        x += 300.0F;
    }
    if (notice_timer_ > 0.0F) {
        renderer.draw_text(kFont, notice_, {size.x / 2.0F, size.y - 60.0F}, 16,
                           {255, 255, 255, 255}, TextAlign::Center);
    }
    if (game_state_ == protocol::GameState::GameOver) {
        renderer.draw_rect({0.0F, size.y * 0.35F, size.x, size.y * 0.3F}, {0, 0, 0, 170});
        renderer.draw_text(kFont, "GAME OVER", {size.x / 2.0F, size.y * 0.42F}, 48,
                           {255, 90, 90, 255}, TextAlign::Center);
        renderer.draw_text(kFont, "A new game starts in a few seconds",
                           {size.x / 2.0F, size.y * 0.54F}, 14, {220, 220, 220, 255},
                           TextAlign::Center);
    }
}

} // namespace ntype::client
