#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include <ntype/engine/util/Log.hpp>
#include <ntype/rtype/Assets.hpp>
#include <ntype/rtype/GameData.hpp>

#include "ClientApp.hpp"
#include "ClientConfig.hpp"

namespace {

constexpr int kExitFailure = 84;

} // namespace

int main(int argc, char** argv) {
    using namespace ntype;
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    const auto config = client::ClientConfig::from_args(args);
    if (!config) {
        std::cerr << "r-type_client: " << config.error() << "\n\n" << client::ClientConfig::usage();
        return kExitFailure;
    }
    if (config->show_help) {
        std::cout << client::ClientConfig::usage();
        return 0;
    }
    const auto assets = rtype::find_assets_directory(config->assets, argv[0]);
    if (!assets) {
        engine::log::error("assets directory not found (use --assets <dir>)");
        return kExitFailure;
    }
    try {
        auto data = rtype::GameData::load_from_file(*assets / "data" / "game.json");
        client::ClientApp app(*config, std::move(data), *assets);
        return app.run();
    } catch (const std::exception& error) {
        engine::log::error("{}", error.what());
        return kExitFailure;
    }
}
