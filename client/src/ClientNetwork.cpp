#include "ClientNetwork.hpp"

#include <algorithm>
#include <array>
#include <system_error>

#include "GameConfig.hpp"

namespace ntype::client {

using namespace std::chrono_literals;
using protocol::MessageType;

ClientNetwork::ClientNetwork(const std::string& host, const std::uint16_t port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)) {
    asio::ip::udp::resolver resolver(io_);
    server_ = *resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port)).begin();
    socket_.non_blocking(true);
}

ClientNetwork::~ClientNetwork() {
    disconnect();
}

void ClientNetwork::send(const std::vector<std::uint8_t>& bytes) {
    std::error_code error;
    socket_.send_to(asio::buffer(bytes), server_, 0, error);
    if (error && error != asio::error::would_block && error != asio::error::try_again) {
        status_ = "Network error: " + error.message();
    }
}

void ClientNetwork::receive() {
    // Bound the work per frame even if a peer floods the socket.
    for (int count = 0; count < 128; ++count) {
        std::array<std::uint8_t, 1500> bytes{};
        asio::ip::udp::endpoint from;
        std::error_code error;
        const auto size = socket_.receive_from(asio::buffer(bytes), from, 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again) {
            break;
        }
        if (error || from != server_) {
            continue;
        }
        handle({bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)});
    }
}

void ClientNetwork::handle(const std::vector<std::uint8_t>& bytes) {
    const auto packet = protocol::decode(bytes);
    if (!packet) {
        return;
    }
    switch (packet->type) {
    case MessageType::ConnectAccept:
        id_ = static_cast<std::uint8_t>(packet->id);
        last_received_ = Clock::now();
        status_ = "Player " + std::to_string(packet->id);
        break;
    case MessageType::ConnectRefuse:
        if (!id_) {
            status_ = "Server full - retrying";
        }
        break;
    case MessageType::WorldSnapshot:
        if (id_) {
            players_.clear();
            for (const auto& entity : packet->entities) {
                if (entity.kind == config::kPlayerKind && entity.id >= 1 &&
                    entity.id <= config::kMaxPlayers) {
                    players_.push_back(entity);
                }
            }
            last_received_ = Clock::now();
        }
        break;
    case MessageType::ClientLeft:
        std::erase_if(players_, [&](const protocol::Entity& player) { return player.id == packet->id; });
        break;
    default:
        break; // Not used by this client yet.
    }
}

void ClientNetwork::reset() {
    id_.reset();
    players_.clear();
}

void ClientNetwork::update(const std::uint8_t input) {
    receive();
    const auto now = Clock::now();
    // The server drops silent clients after 5 s; do the same on our side.
    if (id_ && now - last_received_ > 5s) {
        reset();
        status_ = "Connection lost - reconnecting";
    }
    if (id_) {
        // Sent every frame, so it is also our heartbeat.
        send(protocol::makeInput(input));
    } else if (now - last_connect_ >= 500ms) {
        send(protocol::makeConnectRequest());
        last_connect_ = now;
    }
}

void ClientNetwork::disconnect() {
    if (!id_) {
        return;
    }
    const auto message = protocol::makeDisconnect();
    // UDP can lose a packet; repeat the disconnect, with the server timeout as fallback.
    for (int copy = 0; copy < 3; ++copy) {
        send(message);
    }
    reset();
}

} // namespace ntype::client
