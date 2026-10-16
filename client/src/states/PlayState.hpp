#pragma once

#include <array>
#include <string>
#include <vector>

#include <ntype/engine/ecs/Registry.hpp>
#include <ntype/engine/events/EventBus.hpp>
#include <ntype/engine/scene/IState.hpp>
#include <ntype/engine/time/Clock.hpp>
#include <ntype/rtype/EntityFactory.hpp>
#include <ntype/rtype/systems/Systems.hpp>

#include "../ClientContext.hpp"
#include "../InputMapper.hpp"
#include "../Starfield.hpp"
#include "../WorldSync.hpp"
#include "../systems/InterpolationSystem.hpp"
#include "../systems/RenderSystem.hpp"

namespace ntype::client {

/// The game itself: sends inputs, mirrors the server world and draws it.
/// The client never decides an outcome; kills and deaths come from the server.
class PlayState final : public engine::scene::IState {
public:
    PlayState(ClientContext& context, const rtype::protocol::ConnectAccept& accept,
              std::vector<rtype::protocol::Message> backlog);

    void handle_event(const engine::platform::InputEvent& event) override;
    void update(float dt) override;
    void render(engine::platform::IRenderer& renderer, float alpha) override;

private:
    void handle(const rtype::protocol::Message& message);
    void show_notice(std::string text);
    void draw_hud(engine::platform::IRenderer& renderer) const;

    ClientContext& context_;
    rtype::protocol::ConnectAccept accept_;
    std::vector<rtype::protocol::Message> backlog_;

    engine::ecs::Registry registry_;
    engine::events::EventBus events_;
    rtype::EntityFactory factory_;
    WorldSync sync_;
    InputMapper input_mapper_;
    Starfield starfield_;
    InterpolationSystem interpolation_;
    rtype::systems::AnimationSystem animation_;
    rtype::systems::LifetimeSystem lifetime_;
    rtype::systems::CleanupSystem cleanup_;
    RenderSystem render_system_;
    std::vector<engine::events::Subscription> subscriptions_;
    engine::time::Clock frame_clock_;

    std::uint32_t client_tick_ = 0;
    float time_ = 0.0F;
    rtype::protocol::GameState game_state_ = rtype::protocol::GameState::Waiting;
    std::vector<rtype::protocol::PlayerState> players_;
    std::string notice_;
    float notice_timer_ = 0.0F;
};

} // namespace ntype::client
