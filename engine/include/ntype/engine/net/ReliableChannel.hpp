#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include <ntype/engine/net/Packet.hpp>
#include <ntype/engine/time/Clock.hpp>

namespace ntype::engine::net {

enum class Delivery : std::uint8_t { Unreliable, Reliable };

/// Sequence numbers, acknowledgements and retransmission for one peer.
///
/// Every packet carries a sequence number plus the latest sequence received
/// from the peer (`ack`) and a bitfield of the 32 before it (`ack_bits`).
/// Reliable packets are re-sent, under a fresh sequence number, until one of
/// their copies is acknowledged. A reliable message can therefore arrive
/// twice, so reliable messages must be idempotent.
class ReliableChannel {
public:
    /// Builds the next outgoing packet and tracks it when reliable.
    [[nodiscard]] Packet make_packet(std::uint8_t type, std::vector<std::byte> payload,
                                     Delivery delivery, time::TimePoint now);

    /// Processes the acks in `header`, records its sequence number and tells
    /// whether the payload should be handled (false for duplicates).
    [[nodiscard]] bool on_received(const PacketHeader& header, time::TimePoint now);

    /// Reliable packets whose acknowledgement is overdue, re-stamped for resend.
    [[nodiscard]] std::vector<Packet> collect_resends(time::TimePoint now);

    [[nodiscard]] std::size_t pending_count() const noexcept { return unacked_.size(); }
    /// Smoothed round-trip time; 0 until the first sample.
    [[nodiscard]] float rtt_ms() const noexcept { return rtt_ms_; }

    /// True when `a` is more recent than `b`, accounting for wrap-around.
    [[nodiscard]] static bool sequence_greater(std::uint16_t a, std::uint16_t b) noexcept;

private:
    struct PendingPacket {
        Packet packet;
        time::TimePoint last_sent;
    };
    struct SentRecord {
        std::uint16_t sequence = 0;
        time::TimePoint sent_at;
        bool valid = false;
    };

    void stamp(PacketHeader& header);
    void process_acks(const PacketHeader& header, time::TimePoint now);
    [[nodiscard]] bool is_acked(std::uint16_t sequence, std::uint16_t ack,
                                std::uint32_t ack_bits) const noexcept;
    [[nodiscard]] time::Duration retransmit_timeout() const noexcept;

    std::uint16_t local_sequence_ = 0;
    std::uint16_t remote_sequence_ = 0;
    std::uint32_t received_bits_ = 0;
    bool has_remote_ = false;
    std::deque<PendingPacket> unacked_;
    std::array<SentRecord, 256> sent_records_{};
    float rtt_ms_ = 0.0F;
};

} // namespace ntype::engine::net
