#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

#include <ntype/engine/net/DecodeError.hpp>
#include <ntype/engine/net/ReliableChannel.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::rtype::protocol {

struct EncodedMessage {
    MessageType type;
    std::vector<std::byte> payload;
};

/// Serialises a message payload (the packet header is added by the channel).
[[nodiscard]] EncodedMessage encode(const Message& message);

/// Parses a payload. Every malformed input yields a DecodeError: wrong size,
/// unknown type, out-of-range enum, too many elements.
[[nodiscard]] std::expected<Message, engine::net::DecodeError>
decode(std::uint8_t type, std::span<const std::byte> payload);

[[nodiscard]] MessageType type_of(const Message& message) noexcept;

/// Reliable messages are re-sent until acknowledged and must be idempotent.
[[nodiscard]] engine::net::Delivery delivery_of(MessageType type) noexcept;

} // namespace ntype::rtype::protocol
