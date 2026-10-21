#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include <asio.hpp>

#include "Protocol.hpp"

namespace ntype::client {

class ClientNetwork {
public:
    ClientNetwork(const std::string& host, std::uint16_t port);
    ~ClientNetwork();
    void update(std::uint8_t input);
    void disconnect();

    const std::optional<protocol::Message>& state() const { return state_; }
    const std::string& status() const { return status_; }

private:
    void send(const protocol::Message& message);
    void receive();

    using Clock = std::chrono::steady_clock;
    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint server_;
    std::uint32_t nonce_;
    std::uint32_t sequence_ = 0;
    std::optional<protocol::Message> state_;
    std::string status_ = "Connecting";
    Clock::time_point last_received_ = Clock::now();
    Clock::time_point last_connect_{};
};

} // namespace ntype::client
