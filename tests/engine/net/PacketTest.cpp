#include <vector>

#include <gtest/gtest.h>

#include <ntype/engine/net/Packet.hpp>

using ntype::engine::net::DecodeError;
using ntype::engine::net::Packet;
using ntype::engine::net::PacketHeader;

namespace {

Packet sample() {
    Packet packet;
    packet.header.type = 5;
    packet.header.flags = 1;
    packet.header.sequence = 42;
    packet.header.ack = 41;
    packet.header.ack_bits = 0xF0F0F0F0;
    packet.payload = {std::byte{1}, std::byte{2}, std::byte{3}};
    return packet;
}

} // namespace

TEST(Packet, RoundTrip) {
    const Packet original = sample();
    const auto bytes = original.encode();
    EXPECT_EQ(bytes.size(), PacketHeader::kSize + 3);
    const auto decoded = Packet::decode(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->header.sequence, 42);
    EXPECT_EQ(decoded->header.ack_bits, 0xF0F0F0F0U);
    EXPECT_EQ(decoded->header.payload_size, 3);
    EXPECT_EQ(decoded->payload, original.payload);
}

TEST(Packet, RejectsMalformedDatagrams) {
    auto bytes = sample().encode();

    EXPECT_EQ(Packet::decode(std::span(bytes).first(5)).error(), DecodeError::Truncated);
    EXPECT_EQ(Packet::decode(std::span(bytes).first(bytes.size() - 1)).error(),
              DecodeError::Truncated);

    auto trailing = bytes;
    trailing.push_back(std::byte{0});
    EXPECT_EQ(Packet::decode(trailing).error(), DecodeError::TrailingData);

    auto magic = bytes;
    magic[0] = std::byte{0};
    EXPECT_EQ(Packet::decode(magic).error(), DecodeError::BadMagic);

    auto version = bytes;
    version[2] = std::byte{99};
    EXPECT_EQ(Packet::decode(version).error(), DecodeError::BadVersion);

    auto oversized = bytes;
    oversized[13] = std::byte{0xFF};
    oversized[14] = std::byte{0xFF};
    EXPECT_EQ(Packet::decode(oversized).error(), DecodeError::OversizedPayload);

    const std::vector<std::byte> huge(PacketHeader::kMaxDatagramSize + 1);
    EXPECT_EQ(Packet::decode(huge).error(), DecodeError::OversizedPayload);
}

TEST(Packet, RandomGarbageNeverCrashes) {
    std::vector<std::byte> garbage;
    for (int length = 0; length < 64; ++length) {
        garbage.push_back(static_cast<std::byte>(length * 37 + 11));
        static_cast<void>(Packet::decode(garbage));
    }
}
