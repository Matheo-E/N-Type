#pragma once

// Private to the engine: the only place Asio types appear.

#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <system_error>

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <ntype/engine/net/Endpoint.hpp>

namespace ntype::engine::net {

[[nodiscard]] asio::ip::udp::endpoint to_asio(const Endpoint& endpoint);
[[nodiscard]] Endpoint from_asio(const asio::ip::udp::endpoint& endpoint);

/// RAII wrapper around an IPv4 Asio UDP socket.
class UdpSocket {
public:
    using ReceiveHandler =
        std::function<void(std::error_code, std::span<const std::byte>, const Endpoint&)>;

    /// Throws std::system_error when the port cannot be bound.
    UdpSocket(asio::io_context& io, std::uint16_t port);

    /// Synchronous send; returns the error instead of throwing.
    std::error_code send_to(std::span<const std::byte> bytes, const Endpoint& to);
    /// Starts one asynchronous receive; the handler must re-arm it.
    void async_receive(ReceiveHandler handler);
    void close();

    [[nodiscard]] std::uint16_t local_port() const;

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_;
    std::array<std::byte, 2048> buffer_{};
};

} // namespace ntype::engine::net
