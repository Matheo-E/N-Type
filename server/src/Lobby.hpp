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
    std::uint32_t score = 0; // Reset when a game starts, kept after game over.
};

// One lobby's rules. No networking: the Server feeds it requests and sends
// what it builds. Members are kept in arrival order; the first one leads.
class Lobby {
public:
    // `data` builds each game; it must outlive the lobby.
    Lobby(std::string code, std::string name, const GameData& data = GameData::none());

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

    // Called when a level starts: spectators (and players who died) get a ship.
    void on_level_start();
    // Moves the game to its next level, then calls on_level_start. After the last
    // level, only on_level_start happens. False when not playing.
    bool advance_level();
    // Back to the waiting room, keeping the scores and the level reached for the
    // results screen. Defeat: every player died (or the game was stopped).
    void end_game(protocol::GameResult result = protocol::GameResult::Defeat);
    // Called by the game when a player earns points. False if no member has this slot.
    bool add_score(std::uint8_t slot, std::uint32_t points);

    // Only players of a running game move.
    void set_input(ClientId client, std::uint8_t keys);
    // Runs the game, then applies what happened: points, deaths (the player watches
    // until the next level), the next level when the boss dies, and the end of the
    // game when every player is dead or the last boss is.
    void step(float dt);
    // Bodies that died since the last call, for the explosions (see EntityDied).
    std::vector<protocol::Entity> take_deaths();
    // Numbers the snapshots: always increasing, across games too.
    std::uint32_t next_frame() { return frame_++; }

    // The running game, or nullptr while waiting (for tests).
    Game* game() { return game_ ? &*game_ : nullptr; }
    std::vector<protocol::Entity> snapshot() const;

    protocol::LobbyInfo info() const;
    protocol::LobbyState state_for(ClientId client) const;

private:
    Member* find(ClientId client);
    bool is_leader(ClientId client) const;

    void apply(const GameEvent& event);

    const GameData* data_;
    std::string code_;
    std::string name_;
    protocol::LobbyPhase phase_ = protocol::LobbyPhase::Waiting;
    std::vector<Member> members_;
    std::optional<Game> game_; // Only while playing.
    std::vector<protocol::Entity> deaths_;
    protocol::GameResult result_ = protocol::GameResult::None;
    std::uint8_t level_reached_ = 0;
    std::uint32_t frame_ = 0;
};

} // namespace ntype::server
