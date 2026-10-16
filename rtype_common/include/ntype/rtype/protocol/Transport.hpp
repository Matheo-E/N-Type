#pragma once

#include <optional>

#include <ntype/engine/net/Connection.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::rtype::protocol {

/// Encodes `message` into a packet for `connection`, reliable or not
/// according to its type.
[[nodiscard]] engine::net::Packet make_packet(engine::net::Connection& connection,
                                              const Message& message, engine::time::TimePoint now);

/// Runs the packet through the connection's channel (acks, duplicates) and
/// decodes it. Returns nullopt for duplicates and malformed payloads.
[[nodiscard]] std::optional<Message> receive(engine::net::Connection& connection,
                                             const engine::net::Packet& packet,
                                             engine::time::TimePoint now);

} // namespace ntype::rtype::protocol
