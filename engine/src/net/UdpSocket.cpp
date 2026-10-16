#include "UdpSocket.hpp"

#include <asio/buffer.hpp>

namespace ntype::engine::net {

asio::ip::udp::endpoint to_asio(const Endpoint& endpoint) {
    return {asio::ip::address_v4(endpoint.address), endpoint.port};
}

Endpoint from_asio(const asio::ip::udp::endpoint& endpoint) {
    const auto address = endpoint.address();
    const std::uint32_t ipv4 = address.is_v4() ? address.to_v4().to_uint() : 0;
    return {ipv4, endpoint.port()};
}

UdpSocket::UdpSocket(asio::io_context& io, const std::uint16_t port)
    : socket_(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)) {}

std::error_code UdpSocket::send_to(const std::span<const std::byte> bytes, const Endpoint& to) {
    std::error_code error;
    socket_.send_to(asio::buffer(bytes.data(), bytes.size()), to_asio(to), 0, error);
    return error;
}

void UdpSocket::async_receive(ReceiveHandler handler) {
    socket_.async_receive_from(
        asio::buffer(buffer_), sender_,
        [this, handler = std::move(handler)](const std::error_code error, const std::size_t size) {
            handler(error, std::span<const std::byte>(buffer_.data(), size), from_asio(sender_));
        });
}

void UdpSocket::close() {
    std::error_code ignored;
    socket_.close(ignored);
}

std::uint16_t UdpSocket::local_port() const {
    return socket_.local_endpoint().port();
}

} // namespace ntype::engine::net
