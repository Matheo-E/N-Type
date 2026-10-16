#pragma once

#include <cstdint>

#include <ntype/engine/ecs/Entity.hpp>
#include <ntype/engine/net/Connection.hpp>

namespace ntype::server {

/// A connected player inside a game instance.
struct ClientSession {
    engine::net::Connection connection;
    std::uint8_t slot = 0;
    engine::ecs::Entity player;
    std::uint32_t last_input_tick = 0;
    bool has_input = false;
};

} // namespace ntype::server
