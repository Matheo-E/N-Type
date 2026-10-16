#pragma once

#include <cstdint>

#include <ntype/engine/net/Endpoint.hpp>
#include <ntype/engine/net/Packet.hpp>
#include <ntype/engine/time/Clock.hpp>

namespace ntype::engine::net {

/// A valid datagram handed from the network thread to the game thread.
struct InboundMessage {
    Endpoint from;
    Packet packet;
    time::TimePoint received_at;
};

/// A packet the game thread wants sent.
struct OutboundMessage {
    Endpoint to;
    Packet packet;
};

/// Counters maintained by the network thread.
struct NetStats {
    std::uint64_t packets_in = 0;
    std::uint64_t packets_out = 0;
    std::uint64_t bytes_in = 0;
    std::uint64_t bytes_out = 0;
    std::uint64_t dropped_malformed = 0;
    std::uint64_t send_errors = 0;
};

} // namespace ntype::engine::net
