#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "Lobby.hpp"

namespace ntype::server {

// Every lobby on the server, by room code.
class Lobbies {
public:
    explicit Lobbies(std::uint32_t seed = std::random_device{}());

    // A new lobby with a fresh random code, or nullptr when the server has
    // too many lobbies. The returned pointer stays valid until the lobby is removed.
    Lobby* create(std::string name);
    Lobby* find(const std::string& code);
    Lobby* lobby_of(ClientId client);
    void remove_empty();
    // Ordered by code.
    std::vector<protocol::LobbyInfo> list() const;
    std::map<std::string, Lobby>& all() { return lobbies_; }

private:
    std::string new_code();

    std::mt19937 rng_;
    std::map<std::string, Lobby> lobbies_;
};

} // namespace ntype::server
