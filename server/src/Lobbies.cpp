#include "Lobbies.hpp"

#include <utility>

namespace ntype::server {

Lobbies::Lobbies(const std::uint32_t seed, const GameData& data) : data_(&data), rng_(seed) {}

std::string Lobbies::new_code() {
    std::uniform_int_distribution<std::size_t> pick(0, protocol::kCodeAlphabet.size() - 1);
    std::string code;
    do {
        code.clear();
        for (std::size_t i = 0; i < protocol::kCodeLength; ++i) {
            code += protocol::kCodeAlphabet[pick(rng_)];
        }
    } while (lobbies_.contains(code));
    return code;
}

Lobby* Lobbies::create(std::string name) {
    if (lobbies_.size() >= protocol::kMaxLobbies) {
        return nullptr;
    }
    std::string code = new_code();
    auto [it, added] = lobbies_.try_emplace(code, code, std::move(name), *data_);
    return &it->second;
}

Lobby* Lobbies::find(const std::string& code) {
    const auto it = lobbies_.find(code);
    return it == lobbies_.end() ? nullptr : &it->second;
}

Lobby* Lobbies::lobby_of(const ClientId client) {
    for (auto& [code, lobby] : lobbies_) {
        if (lobby.has(client)) {
            return &lobby;
        }
    }
    return nullptr;
}

void Lobbies::remove_empty() {
    std::erase_if(lobbies_, [](const auto& entry) { return entry.second.empty(); });
}

std::vector<protocol::LobbyInfo> Lobbies::list() const {
    std::vector<protocol::LobbyInfo> infos;
    for (const auto& [code, lobby] : lobbies_) {
        infos.push_back(lobby.info());
    }
    return infos;
}

} // namespace ntype::server
