#include "ServerApp.hpp"

#include <exception>

#include <ntype/engine/util/Log.hpp>

namespace ntype::server {

namespace {

RoomConfig make_room_config(const ServerConfig& config, const std::uint32_t seed) {
    RoomConfig rooms;
    rooms.max_rooms = config.max_rooms;
    rooms.base_seed = seed;
    rooms.instance.tick_rate = config.tick_rate;
    rooms.instance.snapshot_interval = config.snapshot_interval;
    rooms.instance.client_timeout = config.client_timeout;
    return rooms;
}

} // namespace

ServerApp::ServerApp(const ServerConfig& config, rtype::GameData data,
                     const std::atomic<bool>& stop_flag)
    : config_(config), data_(std::move(data)), stop_flag_(stop_flag),
      network_(config.port, inbound_, outbound_),
      rooms_(data_, outbound_, make_room_config(config, config.seed.value_or(0))),
      loop_(engine::time::Duration(1.0 / config.tick_rate)) {}

int ServerApp::run() {
    network_.start();
    engine::log::info("listening on UDP port {} at {} Hz", network_.local_port(),
                      config_.tick_rate);
    loop_.run([this](const float dt) { tick(dt); });
    network_.stop();
    const auto stats = network_.stats();
    engine::log::info("stopped: {} packets in, {} out, {} malformed dropped", stats.packets_in,
                      stats.packets_out, stats.dropped_malformed);
    return 0;
}

void ServerApp::tick(const float dt) {
    if (stop_flag_.load()) {
        loop_.stop();
        return;
    }
    const auto now = engine::time::SteadyClock::now();
    // One bad message or one failing tick must never take the server down.
    for (const auto& message : inbound_.drain()) {
        try {
            rooms_.handle(message);
        } catch (const std::exception& error) {
            engine::log::error("message from {} failed: {}", message.from.to_string(),
                               error.what());
        }
    }
    try {
        rooms_.tick(dt, now);
    } catch (const std::exception& error) {
        engine::log::error("tick failed: {}", error.what());
    }
}

} // namespace ntype::server
