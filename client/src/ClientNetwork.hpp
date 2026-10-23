#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <asio.hpp>

#include "Protocol.hpp"

namespace ntype::client {

class ClientNetwork {
public:
    ClientNetwork(const std::string& host, std::uint16_t port);
    ~ClientNetwork();
    void update(std::uint8_t input);
    void disconnect();

    // Our player id (1 to 4), once the server has accepted us.
    std::optional<std::uint8_t> id() const { return id_; }
    // Players from the latest snapshot; each entity id is a player id.
    const std::vector<protocol::Entity>& players() const { return players_; }
    const std::string& status() const { return status_; }

private:
    using Clock = std::chrono::steady_clock;

    void send(const std::vector<std::uint8_t>& bytes);
    void receive();
    void handle(const std::vector<std::uint8_t>& bytes);
    void reset();

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint server_;
    std::optional<std::uint8_t> id_;
    std::vector<protocol::Entity> players_;
    std::string status_ = "Connecting";
    Clock::time_point last_received_ = Clock::now();
    Clock::time_point last_connect_{};
};

} // namespace ntype::client
