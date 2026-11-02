#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <vector>

#include "Game.hpp"
#include "GameConfig.hpp"
#include "NetworkThread.hpp"
#include "Protocol.hpp"

namespace ntype::server {

using Clock = std::chrono::steady_clock;

struct Player {
    std::uint8_t id = 0; // 1 to 4, also the ship's slot in the game
    Clock::time_point last_seen;
};

// Authoritative game: clients only send inputs, the server moves everyone
// and broadcasts the result every tick. Runs on the main thread; all socket
// work happens on the NetworkThread.
class Server {
public:
    explicit Server(std::uint16_t port);

    // Runs the fixed-rate loop until `running` becomes false.
    void run(const std::atomic<bool>& running);

private:
    void tick();
    void handle(const Datagram& datagram);
    void on_connect(const asio::ip::udp::endpoint& from);
    void remove(const asio::ip::udp::endpoint& from, const char* reason);
    void drop_timed_out();
    void broadcast(const std::vector<std::uint8_t>& data);
    void broadcast_snapshot();

    NetworkThread network_;
    Game game_;
    std::map<asio::ip::udp::endpoint, Player> players_;
    std::uint32_t tick_ = 0;
};

} // namespace ntype::server
