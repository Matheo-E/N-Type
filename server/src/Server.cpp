#include "Server.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <thread>

namespace ntype::server {

namespace {

using protocol::MessageType;

constexpr int kTickRate = 60;
constexpr float kDt = 1.0F / kTickRate;
constexpr auto kTimeout = std::chrono::seconds(5);

} // namespace

Server::Server(const std::uint16_t port) : network_(port) {
    std::cout << "[server] listening on UDP port " << port << std::endl;
}

void Server::run(const std::atomic<bool>& running) {
    const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kDt));
    auto next = Clock::now();
    while (running) {
        tick();
        next += step;
        std::this_thread::sleep_until(next);
    }
    std::cout << "[server] shutting down" << std::endl;
}

void Server::tick() {
    for (const auto& datagram : network_.poll()) {
        handle(datagram);
    }
    drop_timed_out();
    game_.step(kDt);
    broadcast_snapshot();
    ++tick_;
}

void Server::handle(const Datagram& datagram) {
    const auto packet = protocol::decode(datagram.data);
    if (!packet) {
        return; // Malformed: ignored.
    }
    if (packet->type == MessageType::ConnectRequest) {
        on_connect(datagram.from);
        return;
    }

    const auto it = players_.find(datagram.from);
    if (it == players_.end()) {
        return; // Unknown client.
    }
    Player& player = it->second;
    player.last_seen = Clock::now();

    switch (packet->type) {
    case MessageType::Input:
        game_.set_input(player.id, packet->value);
        break;
    case MessageType::Disconnect:
        remove(datagram.from, "disconnected");
        break;
    default:
        break; // Server-to-client messages are not expected here.
    }
}

void Server::on_connect(const asio::ip::udp::endpoint& from) {
    // A repeated ConnectRequest means our ConnectAccept was lost: answer it again.
    if (const auto it = players_.find(from); it != players_.end()) {
        network_.send(from, protocol::makeConnectAccept(it->second.id));
        return;
    }
    if (players_.size() >= config::kMaxPlayers) {
        network_.send(from, protocol::makeConnectRefuse(protocol::RefuseReason::ServerFull));
        return;
    }

    // Lowest free id, so each slot keeps its own colour on the client.
    std::array<bool, config::kMaxPlayers> used{};
    for (const auto& [endpoint, player] : players_) {
        used[player.id - 1U] = true;
    }
    const auto id = static_cast<std::uint8_t>(std::find(used.begin(), used.end(), false) - used.begin() + 1);

    Player player;
    player.id = id;
    game_.add_ship(id);
    player.last_seen = Clock::now();
    players_.emplace(from, player);

    network_.send(from, protocol::makeConnectAccept(id));
    std::cout << "[server] player " << int(id) << " joined from " << from << std::endl;
}

void Server::remove(const asio::ip::udp::endpoint& from, const char* reason) {
    const auto it = players_.find(from);
    if (it == players_.end()) {
        return;
    }
    const auto id = it->second.id;
    game_.remove_ship(id);
    players_.erase(it);
    broadcast(protocol::makeClientLeft(id));
    std::cout << "[server] player " << int(id) << ' ' << reason << std::endl;
}

void Server::drop_timed_out() {
    const auto now = Clock::now();
    std::vector<asio::ip::udp::endpoint> expired;
    for (const auto& [endpoint, player] : players_) {
        if (now - player.last_seen > kTimeout) {
            expired.push_back(endpoint);
        }
    }
    for (const auto& endpoint : expired) {
        remove(endpoint, "timed out");
    }
}

void Server::broadcast(const std::vector<std::uint8_t>& data) {
    for (const auto& [endpoint, player] : players_) {
        network_.send(endpoint, data);
    }
}

void Server::broadcast_snapshot() {
    if (players_.empty()) {
        return;
    }
    // Each player is an entity whose id is its player id.
    broadcast(protocol::makeWorldSnapshot(game_.snapshot()));
}

} // namespace ntype::server
