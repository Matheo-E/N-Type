#pragma once

#include <cstddef>
#include <cstdint>

#include <ntype/engine/net/ByteBuffer.hpp>

namespace ntype::engine::net {

namespace packet_flags {
/// The sender retransmits this packet until it is acknowledged.
inline constexpr std::uint8_t kReliable = 1U << 0U;
/// `ack` and `ack_bits` are meaningful (the sender has received something).
inline constexpr std::uint8_t kHasAck = 1U << 1U;
} // namespace packet_flags

/// Fixed header at the start of every datagram (15 bytes, little-endian).
struct PacketHeader {
    static constexpr std::uint16_t kMagic = 0x4E54; // "NT"
    static constexpr std::uint8_t kVersion = 1;
    static constexpr std::size_t kSize = 15;
    /// Stays under the common 1500-byte MTU once IP/UDP headers are added.
    static constexpr std::size_t kMaxDatagramSize = 1400;
    static constexpr std::size_t kMaxPayloadSize = kMaxDatagramSize - kSize;

    std::uint16_t magic = kMagic;
    std::uint8_t version = kVersion;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    std::uint16_t sequence = 0;
    std::uint16_t ack = 0;
    std::uint32_t ack_bits = 0;
    std::uint16_t payload_size = 0;

    [[nodiscard]] bool is_reliable() const noexcept {
        return (flags & packet_flags::kReliable) != 0;
    }
    [[nodiscard]] bool has_ack() const noexcept { return (flags & packet_flags::kHasAck) != 0; }

    void write(ByteBuffer& buffer) const;
    /// Validates magic, version and payload size.
    [[nodiscard]] static std::expected<PacketHeader, DecodeError> read(ByteBuffer& buffer);

    friend bool operator==(const PacketHeader&, const PacketHeader&) = default;
};

} // namespace ntype::engine::net
