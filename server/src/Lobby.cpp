#include "Lobby.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "GameConfig.hpp"

namespace ntype::server {

using protocol::LobbyPhase;
using protocol::MemberRole;

Lobby::Lobby(std::string code, std::string name) : code_(std::move(code)), name_(std::move(name)) {}

bool Lobby::has(const ClientId client) const {
    return std::any_of(members_.begin(), members_.end(), [client](const Member& m) { return m.client == client; });
}

Member* Lobby::find(const ClientId client) {
    const auto it = std::find_if(members_.begin(), members_.end(), [client](const Member& m) { return m.client == client; });
    return it == members_.end() ? nullptr : &*it;
}

bool Lobby::is_leader(const ClientId client) const {
    return !members_.empty() && members_.front().client == client;
}

bool Lobby::join(const ClientId client, std::string name) {
    if (has(client)) {
        return true;
    }
    if (members_.size() >= config::kMaxPlayers) {
        return false;
    }
    // Lowest free slot, so each slot keeps its own colour on the client.
    std::array<bool, config::kMaxPlayers> used{};
    for (const auto& member : members_) {
        used[member.slot - 1U] = true;
    }
    const auto slot = static_cast<std::uint8_t>(std::find(used.begin(), used.end(), false) - used.begin() + 1);
    const MemberRole role =
        phase_ == LobbyPhase::Playing ? MemberRole::Spectator : MemberRole::Player;
    members_.push_back({client, std::move(name), slot, role});
    return true;
}

void Lobby::leave(const ClientId client) {
    const auto it = std::find_if(members_.begin(), members_.end(), [client](const Member& m) { return m.client == client; });
    if (it == members_.end()) {
        return;
    }
    if (game_) {
        game_->remove_ship(it->slot);
    }
    members_.erase(it);
    const bool players_left = std::any_of(members_.begin(), members_.end(),
                                          [](const Member& m) { return m.role == MemberRole::Player; });
    if (phase_ == LobbyPhase::Playing && !players_left) {
        end_game();
    }
}

bool Lobby::rename(const ClientId by, std::string name) {
    if (!is_leader(by) || phase_ != LobbyPhase::Waiting) {
        return false;
    }
    name_ = std::move(name);
    return true;
}

bool Lobby::start(const ClientId by) {
    if (!is_leader(by) || phase_ != LobbyPhase::Waiting) {
        return false;
    }
    phase_ = LobbyPhase::Playing;
    game_.emplace();
    for (auto& member : members_) {
        member.role = MemberRole::Player;
        game_->add_ship(member.slot);
    }
    return true;
}

void Lobby::on_level_start() {
    if (!game_) {
        return;
    }
    for (auto& member : members_) {
        if (member.role == MemberRole::Spectator) {
            member.role = MemberRole::Player;
            game_->add_ship(member.slot);
        }
    }
}

void Lobby::end_game() {
    phase_ = LobbyPhase::Waiting;
    game_.reset();
    for (auto& member : members_) {
        member.role = MemberRole::Player;
    }
}

void Lobby::set_input(const ClientId client, const std::uint8_t keys) {
    const Member* member = find(client);
    if (game_ && member && member->role == MemberRole::Player) {
        game_->set_input(member->slot, keys);
    }
}

void Lobby::step(const float dt) {
    if (game_) {
        game_->step(dt);
    }
}

std::vector<protocol::Entity> Lobby::snapshot() const {
    return game_ ? game_->snapshot() : std::vector<protocol::Entity>{};
}

protocol::LobbyInfo Lobby::info() const {
    return {code_, name_, static_cast<std::uint8_t>(members_.size()), phase_};
}

protocol::LobbyState Lobby::state_for(const ClientId client) const {
    protocol::LobbyState state{code_, name_, phase_, 0, 0, {}};
    for (const auto& member : members_) {
        if (member.client == client) {
            state.yourSlot = member.slot;
        }
        state.members.push_back({member.slot, member.role, member.name});
    }
    if (!members_.empty()) {
        state.leaderSlot = members_.front().slot;
    }
    return state;
}

} // namespace ntype::server
