#pragma once

#include <filesystem>

#include <ntype/engine/scene/StateMachine.hpp>
#include <ntype/engine/sfml/SfmlAudio.hpp>
#include <ntype/engine/sfml/SfmlInput.hpp>
#include <ntype/engine/sfml/SfmlRenderer.hpp>
#include <ntype/engine/sfml/SfmlWindow.hpp>
#include <ntype/engine/time/GameLoop.hpp>
#include <ntype/rtype/GameData.hpp>

#include "ClientConfig.hpp"
#include "ClientContext.hpp"
#include "ClientNetwork.hpp"

namespace ntype::client {

class ClientApp {
public:
    ClientApp(ClientConfig config, rtype::GameData data, std::filesystem::path assets);

    /// Runs until the window is closed. Returns the process exit code.
    int run();

private:
    void load_assets();
    void fixed_update(float dt);
    void render(float alpha);

    ClientConfig config_;
    rtype::GameData data_;
    std::filesystem::path assets_;
    engine::sfml::SfmlWindow window_;
    engine::sfml::SfmlRenderer renderer_;
    engine::sfml::SfmlAudio audio_;
    engine::sfml::SfmlInput input_;
    ClientNetwork network_;
    engine::scene::StateMachine states_;
    engine::time::GameLoop loop_;
    ClientContext context_;
    engine::time::Clock uptime_;
};

} // namespace ntype::client
