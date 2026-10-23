#include "Server.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <thread>

#include "Factory.hpp"
#include "System.hpp"

namespace ntype::server {

namespace {

using protocol::InputType;
using protocol::MessageType;

constexpr int kTickRate = 60;
constexpr float kDt = 1.0F / kTickRate;
constexpr auto kTimeout = std::chrono::seconds(5);

constexpr float kMaxX = static_cast<float>(config::kWidth) - config::kPlayerSize;
constexpr float kMaxY = static_cast<float>(config::kHeight) - config::kPlayerSize;

bool pressed(const std::uint8_t keys, const InputType key) {
    return (keys & static_cast<std::uint8_t>(key)) != 0;
}

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
    move_players();
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
        player.input = packet->value;
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
    player.entity = Create_E(world_);
    world_.Positions[player.entity].Xy = {100.0F, 150.0F + 120.0F * static_cast<float>(id - 1)};
    world_.Velocities[player.entity];
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
    world_.Positions.erase(it->second.entity);
    world_.Velocities.erase(it->second.entity);
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

void Server::move_players() {
    for (auto& [endpoint, player] : players_) {
        float dx = 0.0F;
        float dy = 0.0F;
        if (pressed(player.input, InputType::Up)) {
            dy -= 1.0F;
        }
        if (pressed(player.input, InputType::Down)) {
            dy += 1.0F;
        }
        if (pressed(player.input, InputType::Left)) {
            dx -= 1.0F;
        }
        if (pressed(player.input, InputType::Right)) {
            dx += 1.0F;
        }
        world_.Velocities[player.entity].Xy = {dx * config::kPlayerSpeed, dy * config::kPlayerSpeed};
    }
    Movement(world_, kDt);
    for (const auto& [endpoint, player] : players_) {
        auto& position = world_.Positions[player.entity].Xy;
        position.x = std::clamp(position.x, 0.0F, kMaxX);
        position.y = std::clamp(position.y, 0.0F, kMaxY);
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
    std::vector<protocol::Entity> entities;
    for (const auto& [endpoint, player] : players_) {
        const auto& position = world_.Positions[player.entity].Xy;
        entities.push_back({player.id, config::kPlayerKind, static_cast<std::int16_t>(position.x),
                            static_cast<std::int16_t>(position.y)});
    }
    broadcast(protocol::makeWorldSnapshot(entities));
}

} // namespace ntype::server
