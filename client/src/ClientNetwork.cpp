#include "ClientNetwork.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <system_error>

#include "GameConfig.hpp"

namespace ntype::client {

using namespace std::chrono_literals;
using protocol::LobbyPhase;
using protocol::MessageType;

namespace {

constexpr auto kConnectRetry = 500ms;
constexpr auto kNoAnswer = 3s;
constexpr auto kListEvery = 1s;
constexpr auto kResend = 250ms;
constexpr auto kGiveUp = 3s;
constexpr auto kTimeout = 5s; // The server drops silent clients after 5 s too.

std::string refuse_text(const std::uint8_t reason) {
    switch (static_cast<protocol::LobbyRefuseReason>(reason)) {
    case protocol::LobbyRefuseReason::NotFound:
        return "No lobby with this code";
    case protocol::LobbyRefuseReason::Full:
        return "This lobby is full";
    case protocol::LobbyRefuseReason::TooManyLobbies:
        return "The server has too many lobbies";
    }
    return "Refused by the server";
}

} // namespace

ClientNetwork::ClientNetwork(const std::string& host, const std::uint16_t port, std::string name)
    : name_(std::move(name)), address_(host + ":" + std::to_string(port)),
      socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)), status_("Connecting to " + address_) {
    asio::ip::udp::resolver resolver(io_);
    server_ = *resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port)).begin();
    socket_.non_blocking(true);
}

ClientNetwork::~ClientNetwork() {
    disconnect();
}

Phase ClientNetwork::phase() const {
    if (!connected_) {
        return Phase::Connecting;
    }
    return lobby_ ? Phase::InLobby : Phase::Browsing;
}

bool ClientNetwork::is_leader() const {
    return lobby_ && lobby_->yourSlot == lobby_->leaderSlot;
}

bool ClientNetwork::is_spectator() const {
    if (!lobby_) {
        return false;
    }
    return std::any_of(lobby_->members.begin(), lobby_->members.end(), [this](const protocol::LobbyMember& m) {
        return m.slot == lobby_->yourSlot && m.role == protocol::MemberRole::Spectator;
    });
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
    if (packet->type == MessageType::ConnectRefuse) {
        if (!connected_) {
            status_ = "Server full - retrying";
        }
        return;
    }
    if (packet->type == MessageType::ConnectAccept && !connected_) {
        connected_ = true;
        status_ = "Connected";
        last_list_ = {};
    }
    if (!connected_) {
        return;
    }
    last_received_ = Clock::now();
    switch (packet->type) {
    case MessageType::LobbyList:
        lobbies_ = packet->lobbies;
        list_received_ = true;
        break;
    case MessageType::LobbyState:
        on_state(packet->lobby);
        break;
    case MessageType::LobbyRefuse:
        error_ = refuse_text(packet->value);
        pending_.reset();
        break;
    case MessageType::WorldSnapshot:
        if (lobby_ && lobby_->state == LobbyPhase::Playing) {
            players_.clear();
            for (const auto& entity : packet->entities) {
                if (entity.kind == config::kPlayerKind && entity.id >= 1 && entity.id <= config::kMaxPlayers) {
                    players_.push_back(entity);
                }
            }
        }
        break;
    case MessageType::ClientLeft:
        std::erase_if(players_, [&](const protocol::Entity& player) { return player.id == packet->id; });
        break;
    default:
        break; // Not used by this client.
    }
}

void ClientNetwork::on_state(const protocol::LobbyState& state) {
    bool accept = false;
    if (lobby_) {
        accept = state.code == lobby_->code;
    } else if (pending_ && pending_->entering) {
        // A late state of the lobby we just left must not pull us back in.
        accept = pending_->enters ? state.code == *pending_->enters : state.code != left_code_;
    } else {
        // The server still counts us as a member: our leave was lost.
        send(protocol::makeLobbyLeave());
        return;
    }
    if (!accept) {
        return;
    }
    lobby_ = state;
    if (state.state != LobbyPhase::Playing) {
        players_.clear();
    }
}

void ClientNetwork::request(const std::function<std::vector<std::uint8_t>()>& make, std::function<bool()> done,
                            const bool entering, std::optional<std::string> code) {
    std::vector<std::uint8_t> bytes;
    try {
        bytes = make();
    } catch (const std::invalid_argument&) {
        error_ = "Invalid name or code";
        return;
    }
    const auto now = Clock::now();
    error_.clear();
    pending_ = Pending{bytes, std::move(done), std::move(code), entering, now, now};
    send(bytes);
}

void ClientNetwork::create(const std::string& name, const std::uint8_t map) {
    request([&] { return protocol::makeLobbyCreate(name, map); }, [this] { return lobby_.has_value(); }, true);
}

void ClientNetwork::join(std::string code) {
    std::transform(code.begin(), code.end(), code.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (code == left_code_) {
        left_code_.clear(); // Joining it again on purpose.
    }
    request([&] { return protocol::makeLobbyJoin(code); }, [this] { return lobby_.has_value(); }, true, code);
}

void ClientNetwork::leave() {
    if (!lobby_) {
        return;
    }
    left_code_ = lobby_->code;
    lobby_.reset();
    players_.clear();
    list_received_ = false;
    request([] { return protocol::makeLobbyLeave(); }, [this] { return list_received_; });
}

void ClientNetwork::rename(const std::string& name) {
    request([&] { return protocol::makeLobbyRename(name); }, [this, name] { return lobby_ && lobby_->name == name; });
}

void ClientNetwork::set_map(const std::uint8_t map) {
    request([&] { return protocol::makeLobbySetMap(map); }, [this, map] { return lobby_ && lobby_->map == map; });
}

void ClientNetwork::start() {
    request([] { return protocol::makeLobbyStart(); },
            [this] { return lobby_ && lobby_->state == LobbyPhase::Playing; });
}

void ClientNetwork::reset() {
    connected_ = false;
    lobbies_.clear();
    lobby_.reset();
    players_.clear();
    pending_.reset();
    left_code_.clear();
}

void ClientNetwork::update(const std::uint8_t input) {
    receive();
    const auto now = Clock::now();
    if (connected_ && now - last_received_ > kTimeout) {
        reset();
        status_ = "Connection lost - reconnecting";
        connect_started_ = now;
    }
    if (!connected_) {
        if (now - last_connect_ >= kConnectRetry) {
            send(protocol::makeConnectRequest(name_));
            last_connect_ = now;
        }
        if (status_.starts_with("Connecting") && now - connect_started_ > kNoAnswer) {
            status_ = "No server answered on " + address_;
        }
        return;
    }
    // Sent every frame, so it is also our heartbeat.
    send(protocol::makeInput(input));
    if (!lobby_ && now - last_list_ >= kListEvery) {
        send(protocol::makeLobbyListRequest());
        last_list_ = now;
    }
    if (pending_) {
        if (pending_->done()) {
            pending_.reset();
        } else if (now - pending_->first_sent > kGiveUp) {
            error_ = "Server did not respond";
            pending_.reset();
        } else if (now - pending_->last_sent >= kResend) {
            send(pending_->bytes);
            pending_->last_sent = now;
        }
    }
}

void ClientNetwork::disconnect() {
    if (!connected_) {
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
