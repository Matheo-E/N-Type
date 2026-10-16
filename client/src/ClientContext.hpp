#pragma once

#include <functional>

#include <ntype/engine/platform/IAudio.hpp>
#include <ntype/engine/platform/IInputProvider.hpp>
#include <ntype/engine/platform/IRenderer.hpp>
#include <ntype/engine/scene/StateMachine.hpp>
#include <ntype/rtype/GameData.hpp>

#include "ClientConfig.hpp"
#include "ClientNetwork.hpp"

namespace ntype::client {

inline constexpr std::string_view kFont = "main";

/// Services shared by every client state. Owned by ClientApp.
struct ClientContext {
    const ClientConfig& config;
    const rtype::GameData& data;
    engine::platform::IRenderer& renderer;
    engine::platform::IAudio& audio;
    engine::platform::IInputProvider& input;
    ClientNetwork& network;
    engine::scene::StateMachine& states;
    std::function<void()> quit;
};

} // namespace ntype::client
