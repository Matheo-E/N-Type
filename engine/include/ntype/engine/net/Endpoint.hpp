#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace ntype::engine::net {

/// IPv4 address and UDP port, both in host byte order.
struct Endpoint {
    std::uint32_t address = 0;
    std::uint16_t port = 0;

    /// "a.b.c.d:port"
    [[nodiscard]] std::string to_string() const;

    /// Resolves a host name or dotted address. Returns nullopt on failure.
    [[nodiscard]] static std::optional<Endpoint> resolve(const std::string& host,
                                                         std::uint16_t port);

    friend bool operator==(const Endpoint&, const Endpoint&) = default;
};

} // namespace ntype::engine::net

template <>
struct std::hash<ntype::engine::net::Endpoint> {
    std::size_t operator()(const ntype::engine::net::Endpoint& endpoint) const noexcept {
        return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(endpoint.address) << 16U) |
                                          endpoint.port);
    }
};
