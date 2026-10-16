#include <atomic>
#include <csignal>
#include <exception>
#include <iostream>
#include <random>
#include <string_view>
#include <vector>

#include <ntype/engine/util/Log.hpp>
#include <ntype/rtype/Assets.hpp>
#include <ntype/rtype/GameData.hpp>

#include "ServerApp.hpp"
#include "ServerConfig.hpp"

namespace {

constexpr int kExitFailure = 84;

std::atomic<bool> g_stop_requested{false};

extern "C" void on_signal(int /*signal*/) {
    g_stop_requested.store(true);
}

} // namespace

int main(int argc, char** argv) {
    using namespace ntype;
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    auto config = server::ServerConfig::from_args(args);
    if (!config) {
        std::cerr << "r-type_server: " << config.error() << "\n\n" << server::ServerConfig::usage();
        return kExitFailure;
    }
    if (config->show_help) {
        std::cout << server::ServerConfig::usage();
        return 0;
    }
    if (!config->seed) {
        config->seed = std::random_device{}();
    }

    const auto assets = rtype::find_assets_directory(config->assets, argv[0]);
    if (!assets) {
        engine::log::error("assets directory not found (use --assets <dir>)");
        return kExitFailure;
    }

    try {
        auto data = rtype::GameData::load_from_file(*assets / "data" / "game.json");
        server::ServerApp app(*config, std::move(data), g_stop_requested);
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);
        return app.run();
    } catch (const std::exception& error) {
        engine::log::error("{}", error.what());
        return kExitFailure;
    }
}
