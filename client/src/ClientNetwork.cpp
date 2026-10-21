#include "ClientNetwork.hpp"

#include <array>
#include <random>
#include <system_error>

namespace ntype::client {

using namespace std::chrono_literals;

ClientNetwork::ClientNetwork(const std::string& host, const std::uint16_t port)
    : socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)),
      nonce_(std::random_device{}()) {
    asio::ip::udp::resolver resolver(io_);
    server_ = *resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port)).begin();
    socket_.non_blocking(true);
}

ClientNetwork::~ClientNetwork() {
    disconnect();
}

void ClientNetwork::send(const protocol::Message& message) {
    const auto bytes = protocol::encode(message);
    std::error_code error;
    socket_.send_to(asio::buffer(bytes), server_, 0, error);
    if (error && error != asio::error::would_block && error != asio::error::try_again) {
        status_ = "Network error: " + error.message();
    }
}

void ClientNetwork::receive() {
    // Bound the work per frame even if a peer floods the socket.
    for (int count = 0; count < 128; ++count) {
        std::array<std::uint8_t, 256> bytes{};
        asio::ip::udp::endpoint from;
        std::error_code error;
        const auto size = socket_.receive_from(asio::buffer(bytes), from, 0, error);
        if (error == asio::error::would_block || error == asio::error::try_again) {
            break;
        }
        if (error || from != server_) {
            continue;
        }
        const auto message = protocol::decode({bytes.data(), size});
        if (!message || message->nonce != nonce_) {
            continue;
        }
        if (message->type == protocol::Type::State) {
            if (state_ && !protocol::newer(message->sequence, state_->sequence)) {
                continue;
            }
            state_ = *message;
            last_received_ = Clock::now();
            status_ = "Player " + std::to_string(message->slot + 1);
        } else if (message->type == protocol::Type::Full && !state_) {
            status_ = "Server full - retrying";
        }
    }
}

void ClientNetwork::update(const std::uint8_t input) {
    receive();
    const auto now = Clock::now();
    if (state_ && now - last_received_ > 3s) {
        state_.reset();
        nonce_ = std::random_device{}();
        status_ = "Connection lost - reconnecting";
    }
    if (state_) {
        protocol::Message message;
        message.type = protocol::Type::Input;
        message.nonce = nonce_;
        message.sequence = ++sequence_;
        message.bits = input;
        send(message);
    } else if (now - last_connect_ >= 500ms) {
        protocol::Message message;
        message.type = protocol::Type::Connect;
        message.nonce = nonce_;
        send(message);
        last_connect_ = now;
    }
}

void ClientNetwork::disconnect() {
    if (!state_) {
        return;
    }
    protocol::Message message;
    message.type = protocol::Type::Disconnect;
    message.nonce = nonce_;
    // UDP can lose a packet; repeat the disconnect, with a timeout as fallback.
    for (int copy = 0; copy < 3; ++copy) {
        send(message);
    }
    state_.reset();
}

} // namespace ntype::client
