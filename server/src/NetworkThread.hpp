#pragma once

#include <array>
#include <cstdint>
#include <thread>
#include <vector>

#include <asio.hpp>

#include "ThreadSafeQueue.hpp"

namespace ntype::server {

struct Datagram {
    asio::ip::udp::endpoint from;
    std::vector<std::uint8_t> data;
};

// Owns the UDP socket and runs Asio on its own thread. The game thread reads
// incoming datagrams with poll() and sends with send(); it never touches the socket.
class NetworkThread {
public:
    explicit NetworkThread(std::uint16_t port);
    ~NetworkThread();

    NetworkThread(const NetworkThread&) = delete;
    NetworkThread& operator=(const NetworkThread&) = delete;

    std::vector<Datagram> poll() { return inbound_.drain(); }
    void send(const asio::ip::udp::endpoint& to, std::vector<std::uint8_t> data);

private:
    void receive();

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_;
    std::array<std::uint8_t, 1500> buffer_{};
    ThreadSafeQueue<Datagram> inbound_;
    std::thread thread_;
};

} // namespace ntype::server
