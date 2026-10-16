#pragma once

#include <optional>
#include <vector>

#include <ntype/engine/net/Connection.hpp>
#include <ntype/engine/net/NetworkThread.hpp>
#include <ntype/rtype/protocol/Messages.hpp>

namespace ntype::client {

/// The client's link to one server: a network thread plus the connection
/// state (acks, resends, keep-alive) kept on the game thread.
class ClientNetwork {
public:
    /// Binds an ephemeral UDP port and starts the network thread.
    ClientNetwork();

    /// Forgets any previous server and targets `server`.
    void connect(const engine::net::Endpoint& server, engine::time::TimePoint now);
    /// Best-effort goodbye, then forgets the server.
    void disconnect(engine::time::TimePoint now);

    void send(const rtype::protocol::Message& message, engine::time::TimePoint now);
    /// Messages received from the server since the last call.
    [[nodiscard]] std::vector<rtype::protocol::Message> poll(engine::time::TimePoint now);
    /// Retransmits overdue reliable packets and keeps the connection alive.
    void update(engine::time::TimePoint now);

    [[nodiscard]] bool timed_out(engine::time::TimePoint now) const;
    [[nodiscard]] float rtt_ms() const noexcept;
    [[nodiscard]] engine::net::NetStats stats() const noexcept { return thread_.stats(); }

private:
    engine::net::ThreadSafeQueue<engine::net::InboundMessage> inbound_;
    engine::net::ThreadSafeQueue<engine::net::OutboundMessage> outbound_;
    engine::net::NetworkThread thread_;
    std::optional<engine::net::Connection> server_;
    engine::time::TimePoint last_ping_;
    engine::time::TimePoint epoch_ = engine::time::SteadyClock::now();
};

} // namespace ntype::client
