#include "server/src/Lobbies.hpp"

#include <cassert>
#include <iostream>
#include <set>

using namespace ntype::server;
using protocol::LobbyPhase;
using protocol::MemberRole;

namespace {

const Member& member(const Lobby& lobby, ClientId client)
{
    for (const auto& m : lobby.members())
        if (m.client == client)
            return m;
    assert(false && "not a member");
    return lobby.members().front();
}

void codes()
{
    Lobbies lobbies(42);
    std::set<std::string> seen;
    for (int i = 0; i < 32; i++) {
        Lobby* lobby = lobbies.create("L");
        assert(lobby && lobby->code().size() == 4);
        for (char c : lobby->code())
            assert(protocol::kCodeAlphabet.find(c) != std::string_view::npos);
        seen.insert(lobby->code());
    }
    assert(seen.size() == 32);
    assert(lobbies.create("L") == nullptr);
}

void slots()
{
    Lobby lobby("K7QX", "L");
    assert(lobby.join(1, "a") && lobby.join(2, "b") && lobby.join(3, "c"));
    assert(member(lobby, 1).slot == 1 && member(lobby, 2).slot == 2 && member(lobby, 3).slot == 3);
    lobby.leave(2);
    assert(lobby.join(4, "d") && member(lobby, 4).slot == 2);
    assert(lobby.join(5, "e"));
    assert(!lobby.join(6, "f"));
    assert(lobby.join(1, "x") && lobby.members().size() == 4);
}

void leader()
{
    Lobby lobby("K7QX", "L");
    lobby.join(1, "a");
    lobby.join(2, "b");
    lobby.join(3, "c");
    assert(lobby.state_for(2).leaderSlot == 1 && lobby.state_for(2).yourSlot == 2);
    lobby.leave(1);
    assert(lobby.members().front().client == 2);
    assert(lobby.state_for(3).leaderSlot == member(lobby, 2).slot);
}

void leader_only()
{
    Lobby lobby("K7QX", "L");
    lobby.join(1, "a");
    lobby.join(2, "b");
    assert(!lobby.rename(2, "B") && !lobby.start(2));
    assert(lobby.rename(1, "B") && lobby.info().name == "B");
    assert(lobby.start(1) && lobby.phase() == LobbyPhase::Playing);
    assert(!lobby.rename(1, "C") && !lobby.start(1));
}

void spectators()
{
    Lobby lobby("K7QX", "L");
    lobby.join(1, "a");
    lobby.join(2, "b");
    lobby.start(1);
    assert(lobby.snapshot().size() == 2);
    lobby.join(3, "c");
    assert(member(lobby, 3).role == MemberRole::Spectator);
    assert(lobby.info().members == 3);
    lobby.set_input(3, static_cast<std::uint8_t>(protocol::InputType::Right));
    lobby.step(1.0F);
    assert(lobby.snapshot().size() == 2);
    lobby.on_level_start();
    assert(member(lobby, 3).role == MemberRole::Player && lobby.snapshot().size() == 3);
}

void end_game()
{
    Lobby lobby("K7QX", "L");
    lobby.join(1, "a");
    lobby.start(1);
    lobby.join(2, "b");
    lobby.end_game();
    assert(lobby.phase() == LobbyPhase::Waiting && lobby.snapshot().empty());
    for (const auto& m : lobby.members())
        assert(m.role == MemberRole::Player);
    lobby.step(1.0F); // waiting: no game, no crash
}

void last_player_leaves()
{
    Lobby lobby("K7QX", "L");
    lobby.join(1, "a");
    lobby.join(2, "b");
    lobby.start(1);
    lobby.join(3, "c");
    lobby.leave(1);
    assert(lobby.phase() == LobbyPhase::Playing);
    lobby.leave(2);
    assert(lobby.phase() == LobbyPhase::Waiting);
    assert(lobby.members().front().client == 3 && member(lobby, 3).role == MemberRole::Player);
}

void registry()
{
    Lobbies lobbies(7);
    Lobby* first = lobbies.create("First");
    Lobby* second = lobbies.create("Second");
    assert(first && second);
    first->join(10, "a");
    assert(lobbies.find(first->code()) == first && lobbies.find("ZZZZ") == nullptr);
    assert(lobbies.lobby_of(10) == first && lobbies.lobby_of(11) == nullptr);
    lobbies.remove_empty();
    std::vector<protocol::LobbyInfo> list = lobbies.list();
    assert(list.size() == 1 && list[0].name == "First" && list[0].members == 1);
}

} // namespace

int main()
{
    codes();
    slots();
    leader();
    leader_only();
    spectators();
    end_game();
    last_player_leaves();
    registry();
    std::cout << "Lobby tests passed!\n";
}
