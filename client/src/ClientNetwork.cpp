#include "ClientNetwork.hpp"

#include <chrono>

#include <ntype/rtype/protocol/Transport.hpp>

namespace ntype::client {

namespace {

constexpr auto kPingInterval = std::chrono::seconds(1);
constexpr auto kServerTimeout = std::chrono::seconds(5);
constexpr int kDisconnectCopies = 3;

} // namespace

ClientNetwork::ClientNetwork() : thread_(0, inbound_, outbound_) {
    thread_.start();
}

void ClientNetwork::connect(const engine::net::Endpoint& server,
                            const engine::time::TimePoint now) {
    server_.emplace(server, now);
    last_ping_ = now;
    static_cast<void>(inbound_.drain());
}

void ClientNetwork::disconnect(const engine::time::TimePoint now) {
    if (!server_) {
        return;
    }
    // Disconnect is unreliable: send a few copies rather than waiting for acks.
    for (int copy = 0; copy < kDisconnectCopies; ++copy) {
        send(rtype::protocol::Disconnect{}, now);
    }
    server_.reset();
}

void ClientNetwork::send(const rtype::protocol::Message& message,
                         const engine::time::TimePoint now) {
    if (server_) {
        outbound_.push({server_->remote(), rtype::protocol::make_packet(*server_, message, now)});
    }
}

std::vector<rtype::protocol::Message> ClientNetwork::poll(const engine::time::TimePoint now) {
    std::vector<rtype::protocol::Message> messages;
    for (const auto& inbound : inbound_.drain()) {
        // Only the server we talk to is trusted.
        if (!server_ || inbound.from != server_->remote()) {
            continue;
        }
        if (auto message = rtype::protocol::receive(*server_, inbound.packet, now)) {
            messages.push_back(std::move(*message));
        }
    }
    return messages;
}

void ClientNetwork::update(const engine::time::TimePoint now) {
    if (!server_) {
        return;
    }
    for (auto& packet : server_->collect_resends(now)) {
        outbound_.push({server_->remote(), std::move(packet)});
    }
    if (now - last_ping_ >= kPingInterval) {
        last_ping_ = now;
        const auto since_start =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - epoch_);
        send(rtype::protocol::Ping{static_cast<std::uint32_t>(since_start.count())}, now);
    }
}

bool ClientNetwork::timed_out(const engine::time::TimePoint now) const {
    return server_ && server_->timed_out(now, kServerTimeout);
}

float ClientNetwork::rtt_ms() const noexcept {
    return server_ ? server_->rtt_ms() : 0.0F;
}

} // namespace ntype::client
