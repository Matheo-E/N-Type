#pragma once

#include <ntype/engine/net/Endpoint.hpp>
#include <ntype/engine/net/ReliableChannel.hpp>

namespace ntype::engine::net {

/// State kept for one remote peer. Owned by the game thread.
class Connection {
public:
    Connection(const Endpoint& remote, time::TimePoint now) noexcept
        : remote_(remote), last_heard_(now) {}

    [[nodiscard]] const Endpoint& remote() const noexcept { return remote_; }

    [[nodiscard]] Packet make_packet(const std::uint8_t type, std::vector<std::byte> payload,
                                     const Delivery delivery, const time::TimePoint now) {
        return channel_.make_packet(type, std::move(payload), delivery, now);
    }

    /// Marks the peer alive and returns whether the payload should be handled.
    [[nodiscard]] bool on_received(const PacketHeader& header, const time::TimePoint now) {
        last_heard_ = now;
        return channel_.on_received(header, now);
    }

    [[nodiscard]] std::vector<Packet> collect_resends(const time::TimePoint now) {
        return channel_.collect_resends(now);
    }

    [[nodiscard]] bool timed_out(const time::TimePoint now,
                                 const time::Duration timeout) const noexcept {
        return now - last_heard_ > timeout;
    }

    [[nodiscard]] float rtt_ms() const noexcept { return channel_.rtt_ms(); }
    [[nodiscard]] const ReliableChannel& channel() const noexcept { return channel_; }

private:
    Endpoint remote_;
    time::TimePoint last_heard_;
    ReliableChannel channel_;
};

} // namespace ntype::engine::net
