#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Game.hpp"
#include "Protocol.hpp"

namespace ntype::server {

// Identifies a connected client independently of its network address.
using ClientId = std::uint32_t;

struct Member {
    ClientId client = 0;
    std::string name;
    std::uint8_t slot = 0; // 1 to 4, the lowest free one on join
    protocol::MemberRole role = protocol::MemberRole::Player;
};

// One lobby's rules. No networking: the Server feeds it requests and sends
// what it builds. Members are kept in arrival order; the first one leads.
class Lobby {
public:
    Lobby(std::string code, std::string name);

    const std::string& code() const { return code_; }
    protocol::LobbyPhase phase() const { return phase_; }
    const std::vector<Member>& members() const { return members_; }
    bool empty() const { return members_.empty(); }
    bool has(ClientId client) const;

    // False when the lobby is full. A member joining again changes nothing.
    // Joining during a game makes a spectator.
    bool join(ClientId client, std::string name);
    // Ends the game if no player remains while playing.
    void leave(ClientId client);
    // Leader only, while waiting.
    bool rename(ClientId by, std::string name);
    bool start(ClientId by);

    // Called by the game when a level starts: spectators get a ship.
    void on_level_start();
    // Called when everyone is game over: back to the waiting room.
    void end_game();

    // Only players of a running game move.
    void set_input(ClientId client, std::uint8_t keys);
    void step(float dt);
    std::vector<protocol::Entity> snapshot() const;

    protocol::LobbyInfo info() const;
    protocol::LobbyState state_for(ClientId client) const;

private:
    Member* find(ClientId client);
    bool is_leader(ClientId client) const;

    std::string code_;
    std::string name_;
    protocol::LobbyPhase phase_ = protocol::LobbyPhase::Waiting;
    std::vector<Member> members_;
    std::optional<Game> game_; // Only while playing.
};

} // namespace ntype::server
