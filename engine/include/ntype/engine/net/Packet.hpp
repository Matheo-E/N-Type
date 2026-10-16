#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

#include <ntype/engine/net/DecodeError.hpp>
#include <ntype/engine/net/PacketHeader.hpp>

namespace ntype::engine::net {

/// One datagram: header plus opaque payload (decoded by the game protocol).
struct Packet {
    PacketHeader header;
    std::vector<std::byte> payload;

    /// Serialises the packet; `header.payload_size` is set from the payload.
    [[nodiscard]] std::vector<std::byte> encode() const;
    /// Never throws: any malformed datagram is reported as a DecodeError.
    [[nodiscard]] static std::expected<Packet, DecodeError>
    decode(std::span<const std::byte> bytes);
};

} // namespace ntype::engine::net
