#include "common/protocol/Protocol.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace protocol;

    std::optional<Packet> request = decode(makeConnectRequest("Ann"));
    assert(request && request->type == MessageType::ConnectRequest && request->text == "Ann");
    assert(!decode({1}));      // the name is required
    assert(makeConnectAccept().size() == 1);
    assert(decode(makeConnectAccept()));
    assert(!decode({2, 1}));   // the old accept carried a player id
    std::optional<Packet> refused = decode(makeConnectRefuse(RefuseReason::ServerFull));
    assert(refused && refused->value == 1);
    assert(decode(makeDisconnect()));

    uint8_t keys = static_cast<uint8_t>(InputType::Right) |
                   static_cast<uint8_t>(InputType::Shoot);
    std::optional<Packet> input = decode(makeInput(keys));
    assert(input && input->value == 24);

    std::optional<Packet> update = decode(makeEntityUpdate(250, -30, 450));
    assert(update && update->id == 250 && update->x == -30 && update->y == 450);

    std::optional<Packet> spawn = decode(makeEntitySpawn(251, 2, 800, 200));
    assert(spawn && spawn->id == 251 && spawn->value == 2 && spawn->x == 800);

    std::optional<Packet> destroy = decode(makeEntityDestroy(251));
    assert(destroy && destroy->id == 251);

    std::vector<Entity> entities = {{1, 1, 100, 200}, {251, 2, -30, 450}};
    std::optional<Packet> snapshot = decode(makeWorldSnapshot(entities));
    assert(snapshot && snapshot->entities.size() == 2);
    assert(snapshot->entities[1].id == 251 && snapshot->entities[1].x == -30);
    assert(decode(makeWorldSnapshot({}))->entities.empty());

    std::optional<Packet> event = decode(makePlayerEvent(2, 1));
    assert(event && event->id == 2 && event->value == 1);
    std::optional<Packet> left = decode(makeClientLeft(2));
    assert(left && left->id == 2);
    std::optional<Packet> state = decode(makeGameStateChange(2));
    assert(state && state->value == 2);

    assert(!decode({}));
    assert(!decode({99}));
    assert(!decode({6, 1, 2}));
    assert(!decode({5, 255}));
    assert(!decode({9, 1}));
    assert(!decode({12, 99}));

    bool rejected = false;
    try { makeWorldSnapshot(std::vector<Entity>(101)); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    // Lobby messages: round trips.
    assert(decode(makeLobbyListRequest())->type == MessageType::LobbyListRequest);
    std::vector<LobbyInfo> infos = {{"K7QX", "Alpha", 2, LobbyPhase::Waiting},
                                    {"23ZZ", "Bob's lobby", 4, LobbyPhase::Playing}};
    assert(makeLobbyList({infos[0]}) ==
           (std::vector<uint8_t>{14, 1, 'K', '7', 'Q', 'X', 5, 'A', 'l', 'p', 'h', 'a', 2, 1}));
    std::optional<Packet> list = decode(makeLobbyList(infos));
    assert(list && list->lobbies.size() == 2 && list->lobbies[1].name == "Bob's lobby" &&
           list->lobbies[1].members == 4 && list->lobbies[1].state == LobbyPhase::Playing);
    assert(decode(makeLobbyList({}))->lobbies.empty());
    assert(makeLobbyCreate("Alpha") == (std::vector<uint8_t>{15, 5, 'A', 'l', 'p', 'h', 'a'}));
    assert(decode(makeLobbyCreate("Alpha"))->text == "Alpha");
    assert(decode(makeLobbyJoin("K7QX"))->code == "K7QX");
    assert(decode(makeLobbyRefuse(LobbyRefuseReason::Full))->value == 2);
    assert(decode(makeLobbyLeave())->type == MessageType::LobbyLeave);
    LobbyState sent{"K7QX", "Alpha", LobbyPhase::Playing, 2, 1,
                    {{1, MemberRole::Player, "Ann"}, {2, MemberRole::Spectator, "Bo"}}};
    assert(makeLobbyState(sent) == (std::vector<uint8_t>{19, 'K', '7', 'Q', 'X', 5, 'A', 'l', 'p', 'h', 'a',
                                                         2, 2, 1, 2, 1, 1, 3, 'A', 'n', 'n',
                                                         2, 2, 2, 'B', 'o'}));
    std::optional<Packet> lobby = decode(makeLobbyState(sent));
    assert(lobby && lobby->lobby.code == "K7QX" && lobby->lobby.yourSlot == 2 &&
           lobby->lobby.leaderSlot == 1 && lobby->lobby.members.size() == 2 &&
           lobby->lobby.members[1].role == MemberRole::Spectator && lobby->lobby.members[1].name == "Bo");
    assert(decode(makeLobbyRename("Beta"))->text == "Beta");
    assert(makeLobbyStart() == std::vector<uint8_t>{21});
    assert(decode(makeLobbyStart())->type == MessageType::LobbyStart);

    // Lobby messages: invalid packets.
    assert(!decode({15, 0}));                     // empty name
    assert(!decode({15, 1, 'A', 0}));             // trailing byte (there is no map)
    assert(!decode({22}));                        // unknown type
    assert(!decode({20, 2, 'A', 0x07}));          // non-printable byte
    assert(!decode({20, 3, 'A', 'B'}));           // length longer than payload
    assert(!decode({16, 'K', '7', 'Q'}));         // short code
    assert(!decode({16, 'k', '7', 'q', 'x'}));    // lowercase is not in the alphabet
    assert(!decode({16, 'O', '0', 'I', '1'}));    // excluded characters
    assert(!decode({14, 33}));                    // more than 32 lobbies
    assert(!decode({17, 4}));                     // unknown refuse reason
    std::vector<uint8_t> badRole = makeLobbyState(sent);
    badRole[badRole.size() - 4] = 3;              // role of the last member
    assert(!decode(badRole));
    std::vector<uint8_t> cut = makeLobbyState(sent);
    cut.pop_back();
    assert(!decode(cut));

    bool threw = false;
    try { makeLobbyRename(std::string(25, 'a')); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    threw = false;
    try { makeLobbyList(std::vector<LobbyInfo>(33, infos[0])); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);

    std::cout << "All 21 protocol messages passed!\n";
}
