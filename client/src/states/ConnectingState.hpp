#pragma once

#include <optional>
#include <string>

#include <ntype/engine/net/Endpoint.hpp>
#include <ntype/engine/scene/IState.hpp>

#include "../ClientContext.hpp"
#include "../Starfield.hpp"

namespace ntype::client {

/// Sends ConnectRequest until the server answers, then starts the game.
class ConnectingState final : public engine::scene::IState {
public:
    /// `notice` is shown above the status line (e.g. "Connection lost").
    ConnectingState(ClientContext& context, std::string notice = {});

    void on_enter() override;
    void handle_event(const engine::platform::InputEvent& event) override;
    void update(float dt) override;
    void render(engine::platform::IRenderer& renderer, float alpha) override;

private:
    ClientContext& context_;
    std::string notice_;
    std::string status_;
    std::optional<engine::net::Endpoint> server_;
    Starfield starfield_;
    float since_request_ = 0.0F;
    unsigned attempts_ = 0;
};

} // namespace ntype::client
