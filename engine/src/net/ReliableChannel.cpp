#include <algorithm>
#include <chrono>

#include <ntype/engine/net/ReliableChannel.hpp>

namespace ntype::engine::net {

namespace {

constexpr std::uint16_t kAckWindow = 32;
constexpr auto kMinRetransmit = std::chrono::milliseconds(100);
constexpr auto kMaxRetransmit = std::chrono::milliseconds(1000);

} // namespace

bool ReliableChannel::sequence_greater(const std::uint16_t a, const std::uint16_t b) noexcept {
    constexpr std::uint16_t kHalf = 32768;
    return ((a > b) && (a - b <= kHalf)) || ((a < b) && (b - a > kHalf));
}

void ReliableChannel::stamp(PacketHeader& header) {
    header.sequence = local_sequence_++;
    header.ack = remote_sequence_;
    header.ack_bits = received_bits_;
    if (has_remote_) {
        header.flags |= packet_flags::kHasAck;
    } else {
        header.flags &= static_cast<std::uint8_t>(~packet_flags::kHasAck);
    }
}

Packet ReliableChannel::make_packet(const std::uint8_t type, std::vector<std::byte> payload,
                                    const Delivery delivery, const time::TimePoint now) {
    Packet packet;
    packet.header.type = type;
    packet.header.payload_size = static_cast<std::uint16_t>(payload.size());
    if (delivery == Delivery::Reliable) {
        packet.header.flags |= packet_flags::kReliable;
    }
    packet.payload = std::move(payload);
    stamp(packet.header);
    sent_records_[packet.header.sequence % sent_records_.size()] = {packet.header.sequence, now,
                                                                    true};
    if (delivery == Delivery::Reliable) {
        unacked_.push_back({packet, now});
    }
    return packet;
}

bool ReliableChannel::on_received(const PacketHeader& header, const time::TimePoint now) {
    if (header.has_ack()) {
        process_acks(header, now);
    }
    const std::uint16_t sequence = header.sequence;
    if (!has_remote_) {
        has_remote_ = true;
        remote_sequence_ = sequence;
        received_bits_ = 0;
        return true;
    }
    if (sequence == remote_sequence_) {
        return false;
    }
    if (sequence_greater(sequence, remote_sequence_)) {
        const auto shift = static_cast<std::uint16_t>(sequence - remote_sequence_);
        // Shift the window forward; the previous latest becomes bit (shift - 1).
        const std::uint64_t shifted =
            shift > kAckWindow ? 0 : (std::uint64_t{received_bits_} << shift);
        const std::uint64_t previous = shift > kAckWindow ? 0 : (std::uint64_t{1} << (shift - 1U));
        received_bits_ = static_cast<std::uint32_t>(shifted | previous);
        remote_sequence_ = sequence;
        return true;
    }
    const auto age = static_cast<std::uint16_t>(remote_sequence_ - sequence);
    if (age > kAckWindow) {
        // Too old to tell whether it is a duplicate. Reliable messages are
        // idempotent, so handling them again is harmless; stale unreliable
        // data is just dropped.
        return header.is_reliable();
    }
    const std::uint32_t bit = 1U << (age - 1U);
    if ((received_bits_ & bit) != 0) {
        return false;
    }
    received_bits_ |= bit;
    return true;
}

bool ReliableChannel::is_acked(const std::uint16_t sequence, const std::uint16_t ack,
                               const std::uint32_t ack_bits) const noexcept {
    if (sequence == ack) {
        return true;
    }
    const auto age = static_cast<std::uint16_t>(ack - sequence);
    return age >= 1 && age <= kAckWindow && (ack_bits & (1U << (age - 1U))) != 0;
}

void ReliableChannel::process_acks(const PacketHeader& header, const time::TimePoint now) {
    SentRecord& record = sent_records_[header.ack % sent_records_.size()];
    if (record.valid && record.sequence == header.ack) {
        const auto sample = std::chrono::duration<float, std::milli>(now - record.sent_at).count();
        rtt_ms_ = rtt_ms_ == 0.0F ? sample : rtt_ms_ * 0.9F + sample * 0.1F;
        record.valid = false;
    }
    std::erase_if(unacked_, [&](const PendingPacket& pending) {
        return is_acked(pending.packet.header.sequence, header.ack, header.ack_bits);
    });
}

time::Duration ReliableChannel::retransmit_timeout() const noexcept {
    const auto twice_rtt = std::chrono::duration<double, std::milli>(2.0 * rtt_ms_);
    return std::clamp<time::Duration>(twice_rtt, kMinRetransmit, kMaxRetransmit);
}

std::vector<Packet> ReliableChannel::collect_resends(const time::TimePoint now) {
    std::vector<Packet> resends;
    const time::Duration timeout = retransmit_timeout();
    for (auto& pending : unacked_) {
        if (now - pending.last_sent < timeout) {
            continue;
        }
        stamp(pending.packet.header);
        sent_records_[pending.packet.header.sequence % sent_records_.size()] = {
            pending.packet.header.sequence, now, true};
        pending.last_sent = now;
        resends.push_back(pending.packet);
    }
    return resends;
}

} // namespace ntype::engine::net
