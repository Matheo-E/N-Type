#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <asio.hpp>

#include "Protocol.hpp"

namespace ntype::client {

enum class Phase { Connecting, Browsing, InLobby };

// What the protocol can carry as a player name: 1 to 16 printable ASCII characters.
bool is_valid_name(const std::string& name);
inline constexpr const char* kInvalidName = "Names are 1 to 16 characters, without accents.";

// The client's session with the server, from the lobby list into the game.
// The server re-sends the lobby state; each request here is re-sent until that
// state shows its effect (or the server refuses it).
class ClientNetwork {
public:
    // Throws std::invalid_argument (kInvalidName) when !is_valid_name(name).
    ClientNetwork(const std::string& host, std::uint16_t port, std::string name);
    ~ClientNetwork();
    // Once per frame: receives, re-sends, and sends `input` (also the heartbeat).
    void update(std::uint8_t input);
    void disconnect();

    Phase phase() const;
    const std::string& name() const { return name_; }
    const std::string& status() const { return status_; }
    // Why the last request failed; empty when none did.
    const std::string& error() const { return error_; }
    void clear_error() { error_.clear(); }
    // A request is waiting for the server.
    bool busy() const { return pending_.has_value(); }

    const std::vector<protocol::LobbyInfo>& lobbies() const { return lobbies_; }
    const std::optional<protocol::LobbyState>& lobby() const { return lobby_; }
    bool is_leader() const;
    bool is_spectator() const;
    // Players from the latest complete frame while our lobby plays; each entity id is a slot.
    const std::vector<protocol::Entity>& players() const { return players_; }
    // Every entity of the latest complete frame (players, enemies, shots, items).
    const std::vector<protocol::Entity>& entities() const { return entities_; }
    // Bodies that died since the last call (EntityDied), for the explosions.
    std::vector<protocol::Entity> take_deaths();

    void create(const std::string& name);
    // Lower case is accepted.
    void join(std::string code);
    void leave();
    void rename(const std::string& name);
    void start();

private:
    using Clock = std::chrono::steady_clock;

    struct Pending {
        std::vector<std::uint8_t> bytes;
        std::function<bool()> done;
        std::optional<std::string> enters; // Create (any code) or Join (that code)
        bool entering = false;
        Clock::time_point first_sent;
        Clock::time_point last_sent;
        bool leaving = false; // Confirmed by the LobbyList that answers it
    };

    void send(const std::vector<std::uint8_t>& bytes);
    void receive();
    void handle(const std::vector<std::uint8_t>& bytes);
    void on_state(const protocol::LobbyState& state);
    void on_snapshot(const protocol::Packet& packet);
    void clear_world();
    void request(const std::function<std::vector<std::uint8_t>()>& make, std::function<bool()> done,
                 bool entering = false, std::optional<std::string> code = std::nullopt);
    void reset();

    std::string name_;
    std::string address_;
    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint server_;
    bool connected_ = false;
    std::vector<protocol::LobbyInfo> lobbies_;
    std::optional<protocol::LobbyState> lobby_;
    std::vector<protocol::Entity> players_;
    std::vector<protocol::Entity> entities_;
    std::vector<protocol::Entity> deaths_;
    // The frame being put together from its parts, and the last one shown.
    std::uint32_t building_frame_ = 0;
    std::vector<std::optional<std::vector<protocol::Entity>>> building_parts_;
    std::optional<std::uint32_t> shown_frame_;
    std::optional<Pending> pending_;
    std::string left_code_;      // The lobby we last left: its late states are ignored.
    bool list_received_ = false; // Confirms a leave.
    std::string status_;
    std::string error_;
    Clock::time_point last_received_ = Clock::now();
    Clock::time_point connect_started_ = Clock::now();
    Clock::time_point last_connect_{};
    Clock::time_point last_list_{};
};

} // namespace ntype::client
