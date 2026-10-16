#pragma once

#include <atomic>

#include <ntype/engine/net/NetworkThread.hpp>
#include <ntype/engine/time/GameLoop.hpp>
#include <ntype/rtype/GameData.hpp>

#include "RoomManager.hpp"
#include "ServerConfig.hpp"

namespace ntype::server {

/// Owns the network thread, the rooms and the fixed-rate main loop.
class ServerApp {
public:
    /// Throws std::system_error when the port cannot be bound.
    ServerApp(const ServerConfig& config, rtype::GameData data, const std::atomic<bool>& stop_flag);

    /// Runs until the stop flag is raised. Returns the process exit code.
    int run();

private:
    void tick(float dt);

    ServerConfig config_;
    rtype::GameData data_;
    const std::atomic<bool>& stop_flag_;
    engine::net::ThreadSafeQueue<engine::net::InboundMessage> inbound_;
    engine::net::ThreadSafeQueue<engine::net::OutboundMessage> outbound_;
    engine::net::NetworkThread network_;
    RoomManager rooms_;
    engine::time::GameLoop loop_;
};

} // namespace ntype::server
