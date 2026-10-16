#include <format>

#include "UdpSocket.hpp"

namespace ntype::engine::net {

std::string Endpoint::to_string() const {
    return std::format("{}.{}.{}.{}:{}", (address >> 24U) & 0xFFU, (address >> 16U) & 0xFFU,
                       (address >> 8U) & 0xFFU, address & 0xFFU, port);
}

std::optional<Endpoint> Endpoint::resolve(const std::string& host, const std::uint16_t port) {
    asio::io_context io;
    asio::ip::udp::resolver resolver(io);
    std::error_code error;
    const auto results = resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port), error);
    if (error || results.empty()) {
        return std::nullopt;
    }
    return from_asio(results.begin()->endpoint());
}

} // namespace ntype::engine::net
