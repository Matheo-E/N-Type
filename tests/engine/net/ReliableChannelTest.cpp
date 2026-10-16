#include <chrono>

#include <gtest/gtest.h>

#include <ntype/engine/net/ReliableChannel.hpp>

using namespace std::chrono_literals;
using ntype::engine::net::Delivery;
using ntype::engine::net::Packet;
using ntype::engine::net::ReliableChannel;
using ntype::engine::time::TimePoint;

namespace {

const TimePoint kStart{};

} // namespace

TEST(ReliableChannel, SequenceComparisonHandlesWrapAround) {
    EXPECT_TRUE(ReliableChannel::sequence_greater(1, 0));
    EXPECT_TRUE(ReliableChannel::sequence_greater(0, 65535));
    EXPECT_FALSE(ReliableChannel::sequence_greater(65535, 0));
    EXPECT_FALSE(ReliableChannel::sequence_greater(5, 5));
}

TEST(ReliableChannel, AcknowledgedReliablePacketIsNotResent) {
    ReliableChannel alice;
    ReliableChannel bob;
    const Packet packet = alice.make_packet(1, {}, Delivery::Reliable, kStart);
    EXPECT_EQ(alice.pending_count(), 1U);
    EXPECT_TRUE(bob.on_received(packet.header, kStart));
    const Packet reply = bob.make_packet(2, {}, Delivery::Unreliable, kStart + 10ms);
    EXPECT_TRUE(alice.on_received(reply.header, kStart + 20ms));
    EXPECT_EQ(alice.pending_count(), 0U);
    EXPECT_GT(alice.rtt_ms(), 0.0F);
    EXPECT_TRUE(alice.collect_resends(kStart + 5s).empty());
}

TEST(ReliableChannel, LostReliablePacketIsResentWithANewSequence) {
    ReliableChannel alice;
    const Packet lost = alice.make_packet(1, {std::byte{9}}, Delivery::Reliable, kStart);
    EXPECT_TRUE(alice.collect_resends(kStart + 10ms).empty());
    const auto resends = alice.collect_resends(kStart + 500ms);
    ASSERT_EQ(resends.size(), 1U);
    EXPECT_NE(resends[0].header.sequence, lost.header.sequence);
    EXPECT_EQ(resends[0].payload, lost.payload);
    EXPECT_TRUE(resends[0].header.is_reliable());
}

TEST(ReliableChannel, UnreliablePacketsAreNeverResent) {
    ReliableChannel alice;
    static_cast<void>(alice.make_packet(1, {}, Delivery::Unreliable, kStart));
    EXPECT_EQ(alice.pending_count(), 0U);
    EXPECT_TRUE(alice.collect_resends(kStart + 10s).empty());
}

TEST(ReliableChannel, DuplicatesAreDetected) {
    ReliableChannel alice;
    ReliableChannel bob;
    const Packet first = alice.make_packet(1, {}, Delivery::Unreliable, kStart);
    const Packet second = alice.make_packet(1, {}, Delivery::Unreliable, kStart);
    EXPECT_TRUE(bob.on_received(second.header, kStart));
    EXPECT_TRUE(bob.on_received(first.header, kStart)); // late but new
    EXPECT_FALSE(bob.on_received(first.header, kStart));
    EXPECT_FALSE(bob.on_received(second.header, kStart));
}

TEST(ReliableChannel, AckBitsCoverOutOfOrderPackets) {
    ReliableChannel alice;
    ReliableChannel bob;
    const Packet a = alice.make_packet(1, {}, Delivery::Reliable, kStart);
    const Packet b = alice.make_packet(1, {}, Delivery::Reliable, kStart);
    const Packet c = alice.make_packet(1, {}, Delivery::Reliable, kStart);
    EXPECT_TRUE(bob.on_received(c.header, kStart));
    EXPECT_TRUE(bob.on_received(a.header, kStart));
    const Packet reply = bob.make_packet(2, {}, Delivery::Unreliable, kStart);
    EXPECT_TRUE(alice.on_received(reply.header, kStart));
    // a and c acknowledged, b still pending.
    EXPECT_EQ(alice.pending_count(), 1U);
    static_cast<void>(b);
}

TEST(ReliableChannel, FirstPacketDoesNotFakeAnAck) {
    ReliableChannel alice;
    ReliableChannel bob;
    static_cast<void>(alice.make_packet(1, {}, Delivery::Reliable, kStart));
    // Bob has received nothing yet: his ack fields must be ignored.
    const Packet hello = bob.make_packet(2, {}, Delivery::Unreliable, kStart);
    EXPECT_FALSE(hello.header.has_ack());
    EXPECT_TRUE(alice.on_received(hello.header, kStart));
    EXPECT_EQ(alice.pending_count(), 1U);
}
