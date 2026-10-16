#pragma once

#include <cstdint>
#include <memory>

#include <ntype/engine/net/NetMessages.hpp>
#include <ntype/engine/net/ThreadSafeQueue.hpp>

namespace ntype::engine::net {

/// Runs the UDP socket on its own thread.
///
/// Incoming datagrams are validated (header, sizes) and pushed to `inbound`;
/// malformed ones are counted and dropped. Packets pushed to `outbound` are
/// sent. No game state is ever touched from this thread.
class NetworkThread {
public:
    /// Binds the socket immediately; port 0 picks a free port.
    /// Throws std::system_error when the port cannot be bound.
    NetworkThread(std::uint16_t port, ThreadSafeQueue<InboundMessage>& inbound,
                  ThreadSafeQueue<OutboundMessage>& outbound);
    ~NetworkThread();

    NetworkThread(const NetworkThread&) = delete;
    NetworkThread& operator=(const NetworkThread&) = delete;
    NetworkThread(NetworkThread&&) = delete;
    NetworkThread& operator=(NetworkThread&&) = delete;

    void start();
    /// Joins the thread. Called by the destructor.
    void stop();

    [[nodiscard]] std::uint16_t local_port() const noexcept;
    [[nodiscard]] NetStats stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ntype::engine::net
