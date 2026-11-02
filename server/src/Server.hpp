#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Console.hpp"
#include "Lobbies.hpp"
#include "NetworkThread.hpp"
#include "Protocol.hpp"

namespace ntype::server {

using Clock = std::chrono::steady_clock;

struct Client {
    ClientId id = 0;
    std::string name;
    Clock::time_point last_seen;
};

// Authoritative server hosting many lobbies, each with its own game. Clients
// only send requests and inputs; the server re-sends the current lobby state
// and snapshots, so a lost packet is replaced by the next one. Runs on the
// main thread; all socket work happens on the NetworkThread.
class Server {
public:
    // Port 0 binds any free port (see port()).
    explicit Server(std::uint16_t port);

    // Runs the fixed-rate loop until `running` becomes false.
    void run(const std::atomic<bool>& running);
    std::uint16_t port() const { return network_.port(); }
    // Commands typed on the console are run each tick (optional).
    void set_console(Console* console) { console_ = console; }

private:
    void tick();
    void handle(const Datagram& datagram);
    void handle_lobby(const asio::ip::udp::endpoint& from, Client& client, const protocol::Packet& packet);
    void on_connect(const asio::ip::udp::endpoint& from, const std::string& name);
    void remove(const asio::ip::udp::endpoint& from, const char* reason);
    void drop_timed_out();
    void run_command(const std::string& line);
    void send_state(Lobby& lobby);
    void send_to_members(const Lobby& lobby, const std::vector<std::uint8_t>& data);

    NetworkThread network_;
    Console* console_ = nullptr;
    Lobbies lobbies_;
    std::map<asio::ip::udp::endpoint, Client> clients_;
    std::map<ClientId, asio::ip::udp::endpoint> endpoints_;
    ClientId next_id_ = 1;
    std::uint32_t tick_ = 0;
};

} // namespace ntype::server
