#include "Server.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "GameConfig.hpp"

namespace ntype::server {

namespace {

using protocol::LobbyRefuseReason;
using protocol::MessageType;

constexpr int kTickRate = 60;
constexpr float kDt = 1.0F / kTickRate;
constexpr auto kTimeout = std::chrono::seconds(5);
constexpr std::uint32_t kStateEvery = 6; // ticks: 10 lobby states per second

GameData load_or_throw(const bool no_enemies, const bool invincible) {
    GameData data;
    data.invincible = invincible;
    if (no_enemies) {
        return data;
    }
    std::string error;
    if (!load_game_data(data, NTYPE_ASSET_DIR, error)) {
        throw std::runtime_error("game data: " + error);
    }
    return data;
}

} // namespace

Server::Server(const std::uint16_t port, const bool no_enemies, const bool invincible)
    : data_(load_or_throw(no_enemies, invincible)), network_(port), lobbies_(std::random_device{}(), data_) {
    std::cout << "[server] " << data_.levels.size() << " levels loaded" << std::endl;
    std::cout << "[server] listening on UDP port " << network_.port() << std::endl;
}

void Server::run(const std::atomic<bool>& running) {
    const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kDt));
    Clock::time_point next = Clock::now();
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
    if (console_) {
        for (const auto& line : console_->poll()) {
            run_command(line);
        }
    }
    drop_timed_out();
    const bool send_states = tick_ % kStateEvery == 0;
    for (auto& [code, lobby] : lobbies_.all()) {
        if (lobby.phase() == protocol::LobbyPhase::Playing) {
            lobby.step(kDt);
            for (const protocol::Entity& body : lobby.take_deaths()) {
                send_to_members(lobby, protocol::makeEntityDied(body));
            }
            for (const auto& part : protocol::makeWorldSnapshots(lobby.next_frame(), lobby.snapshot())) {
                send_to_members(lobby, part);
            }
        }
        if (send_states) {
            send_state(lobby);
        }
    }
    ++tick_;
}

void Server::handle(const Datagram& datagram) {
    const std::optional<protocol::Packet> packet = protocol::decode(datagram.data);
    if (!packet) {
        return; // Malformed: ignored.
    }
    if (packet->type == MessageType::ConnectRequest) {
        on_connect(datagram.from, packet->text);
        return;
    }

    const auto it = clients_.find(datagram.from);
    if (it == clients_.end()) {
        return; // Unknown client.
    }
    Client& client = it->second;
    client.last_seen = Clock::now();

    switch (packet->type) {
    case MessageType::Input:
        if (Lobby* lobby = lobbies_.lobby_of(client.id)) {
            lobby->set_input(client.id, packet->value);
        }
        break;
    case MessageType::Disconnect:
        remove(datagram.from, "disconnected");
        break;
    default:
        handle_lobby(datagram.from, client, *packet);
        break;
    }
}

// Every request is safe to repeat: the client re-sends it until the lobby
// state shows its effect.
void Server::handle_lobby(const asio::ip::udp::endpoint& from, Client& client, const protocol::Packet& packet) {
    Lobby* lobby = lobbies_.lobby_of(client.id);
    const auto reply_state = [&](const Lobby& target) {
        network_.send(from, protocol::makeLobbyState(target.state_for(client.id)));
    };
    const auto refuse = [&](const LobbyRefuseReason reason) {
        network_.send(from, protocol::makeLobbyRefuse(reason));
    };

    switch (packet.type) {
    case MessageType::LobbyListRequest:
        network_.send(from, protocol::makeLobbyList(lobbies_.list()));
        break;
    case MessageType::LobbyCreate:
        if (!lobby) {
            lobby = lobbies_.create(packet.text);
            if (!lobby) {
                refuse(LobbyRefuseReason::TooManyLobbies);
                return;
            }
            lobby->join(client.id, client.name);
            std::cout << "[server] " << client.name << " created lobby " << lobby->code() << std::endl;
        }
        reply_state(*lobby);
        break;
    case MessageType::LobbyJoin:
        if (!lobby) {
            lobby = lobbies_.find(packet.code);
            if (!lobby) {
                refuse(LobbyRefuseReason::NotFound);
                return;
            }
            if (!lobby->join(client.id, client.name)) {
                refuse(LobbyRefuseReason::Full);
                return;
            }
            std::cout << "[server] " << client.name << " joined lobby " << lobby->code() << std::endl;
        }
        reply_state(*lobby);
        break;
    case MessageType::LobbyLeave:
        // Always answered with the list: it tells the client its leave arrived.
        if (lobby) {
            lobby->leave(client.id);
            lobbies_.remove_empty();
        }
        network_.send(from, protocol::makeLobbyList(lobbies_.list()));
        break;
    case MessageType::LobbyRename:
    case MessageType::LobbyStart:
        if (!lobby) {
            return;
        }
        if (packet.type == MessageType::LobbyRename) {
            lobby->rename(client.id, packet.text);
        } else {
            lobby->start(client.id);
        }
        reply_state(*lobby);
        break;
    default:
        break; // Server-to-client messages are not expected here.
    }
}

void Server::on_connect(const asio::ip::udp::endpoint& from, const std::string& name) {
    // A repeated ConnectRequest means our ConnectAccept was lost: answer it again.
    if (const auto it = clients_.find(from); it != clients_.end()) {
        it->second.last_seen = Clock::now();
        network_.send(from, protocol::makeConnectAccept());
        return;
    }
    if (clients_.size() >= config::kMaxClients) {
        network_.send(from, protocol::makeConnectRefuse(protocol::RefuseReason::ServerFull));
        return;
    }
    const Client client{next_id_++, name, Clock::now()};
    clients_.emplace(from, client);
    endpoints_.emplace(client.id, from);
    network_.send(from, protocol::makeConnectAccept());
    std::cout << "[server] " << name << " connected from " << from << std::endl;
}

void Server::remove(const asio::ip::udp::endpoint& from, const char* reason) {
    const auto it = clients_.find(from);
    if (it == clients_.end()) {
        return;
    }
    const Client client = it->second;
    clients_.erase(it);
    endpoints_.erase(client.id);
    if (Lobby* lobby = lobbies_.lobby_of(client.id)) {
        const bool playing = lobby->phase() == protocol::LobbyPhase::Playing;
        std::uint8_t slot = 0;
        for (const auto& member : lobby->members()) {
            if (member.client == client.id) {
                slot = member.slot;
            }
        }
        lobby->leave(client.id);
        if (playing) {
            send_to_members(*lobby, protocol::makeClientLeft(slot));
        }
        lobbies_.remove_empty();
    }
    std::cout << "[server] " << client.name << ' ' << reason << std::endl;
}

void Server::drop_timed_out() {
    const Clock::time_point now = Clock::now();
    std::vector<asio::ip::udp::endpoint> expired;
    for (const auto& [endpoint, client] : clients_) {
        if (now - client.last_seen > kTimeout) {
            expired.push_back(endpoint);
        }
    }
    for (const auto& endpoint : expired) {
        remove(endpoint, "timed out");
    }
}

// Debug commands until levels and game over exist: "level <code>", "gameover <code>".
void Server::run_command(const std::string& line) {
    std::istringstream words(line);
    std::string command;
    std::string code;
    words >> command >> code;
    if (command.empty()) {
        return; // Blank line.
    }
    std::transform(code.begin(), code.end(), code.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::toupper(c)); });
    Lobby* lobby = lobbies_.find(code);
    if ((command != "level" && command != "gameover" && command != "score") || !lobby) {
        std::cout << "[server] unknown command: " << line
                  << " (level <code> | gameover <code> | score <code> <slot> <points>)" << std::endl;
        return;
    }
    if (command == "score") {
        // Until enemies exist: gives points by hand to try the score display.
        unsigned slot = 0;
        std::uint32_t points = 0;
        if (!(words >> slot >> points) || slot < 1 || slot > config::kMaxPlayers ||
            !lobby->add_score(static_cast<std::uint8_t>(slot), points)) {
            std::cout << "[server] usage: score <code> <slot 1-4 of a member> <points>" << std::endl;
            return;
        }
        std::cout << "[server] slot " << slot << " +" << points << " in " << code << std::endl;
    } else if (command == "level") {
        // Skips to the next level (stays on the last one): spectators get a ship.
        lobby->advance_level();
        std::cout << "[server] next level in " << code << std::endl;
    } else {
        lobby->end_game();
        std::cout << "[server] game over in " << code << std::endl;
    }
    send_state(*lobby);
}

void Server::send_state(Lobby& lobby) {
    for (const auto& member : lobby.members()) {
        if (const auto it = endpoints_.find(member.client); it != endpoints_.end()) {
            network_.send(it->second, protocol::makeLobbyState(lobby.state_for(member.client)));
        }
    }
}

void Server::send_to_members(const Lobby& lobby, const std::vector<std::uint8_t>& data) {
    for (const auto& member : lobby.members()) {
        if (const auto it = endpoints_.find(member.client); it != endpoints_.end()) {
            network_.send(it->second, data);
        }
    }
}

} // namespace ntype::server
