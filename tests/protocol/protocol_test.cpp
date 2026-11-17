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

    std::optional<Packet> update = decode(makeEntityUpdate(250, 150000, -30));
    assert(update && update->id == 250 && update->x == 150000 && update->y == -30);

    std::optional<Packet> spawn = decode(makeEntitySpawn(251, 2, 800, 200));
    assert(spawn && spawn->id == 251 && spawn->value == 2 && spawn->x == 800);

    std::optional<Packet> destroy = decode(makeEntityDestroy(251));
    assert(destroy && destroy->id == 251);

    // A snapshot part: frame, part, parts, then 14-byte entities (id, kind, type, hp, hits, x, y).
    std::vector<Entity> entities = {{1, 1, 100, 200, 0, 100, 0}, {251, 2, 150000, -450, 4, 37, 200}};
    assert(makeWorldSnapshot(7, 0, 1, {entities[1]}) ==
           (std::vector<uint8_t>{9, 7, 0, 0, 0, 0, 1, 1, 251, 0, 2, 4, 37, 200,
                                 0xF0, 0x49, 0x02, 0x00, 0x3E, 0xFE, 0xFF, 0xFF}));
    std::optional<Packet> snapshot = decode(makeWorldSnapshot(7, 0, 1, entities));
    assert(snapshot && snapshot->frame == 7 && snapshot->part == 0 && snapshot->parts == 1);
    assert(snapshot->entities.size() == 2 && snapshot->entities[1].id == 251 &&
           snapshot->entities[1].x == 150000 && snapshot->entities[1].y == -450 &&
           snapshot->entities[1].type == 4 && snapshot->entities[1].hp == 37 &&
           snapshot->entities[1].hits == 200);
    assert(decode(makeWorldSnapshot(8, 0, 1, {}))->entities.empty());
    entities[0].type = kGoldenPlayer;
    const auto golden = decode(makeWorldSnapshot(8, 0, 1, entities));
    assert(golden && golden->entities[0].type == kGoldenPlayer && golden->entities[0].hp == 100);

    // A big frame is split into parts of at most 90 entities (1268 bytes), which add back up.
    std::vector<Entity> crowd(200, Entity{300, 3, 10, 20, 0, 0, 0});
    std::vector<std::vector<uint8_t>> parts = makeWorldSnapshots(9, crowd);
    assert(parts.size() == 3 && makeWorldSnapshots(9, {}).size() == 1);
    size_t total = 0;
    for (size_t i = 0; i < parts.size(); i++) {
        std::optional<Packet> part = decode(parts[i]);
        assert(part && part->frame == 9 && part->part == i && part->parts == 3 && parts[i].size() <= 1300);
        total += part->entities.size();
    }
    assert(total == 200);

    // EntityDied: the same 14-byte entity.
    std::optional<Packet> died = decode(makeEntityDied(entities[1]));
    assert(died && died->type == MessageType::EntityDied && died->entities.size() == 1 &&
           died->entities[0].id == 251 && died->entities[0].type == 4 && died->entities[0].x == 150000);

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
    assert(!decode({9, 0, 0, 0, 0, 1, 1, 0}));    // part 1 of a 1-part frame
    assert(!decode({9, 0, 0, 0, 0, 0, 1, 91}));   // more than 90 entities in a part
    std::vector<uint8_t> badKind = makeWorldSnapshot(1, 0, 1, {entities[0]});
    badKind[10] = 6;                              // kind 6 does not exist
    assert(!decode(badKind));
    assert(!decode({12, 99}));

    bool rejected = false;
    try { makeWorldSnapshot(0, 0, 1, std::vector<Entity>(91)); }
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
                    {{1, MemberRole::Player, "Ann", 70000}, {2, MemberRole::Spectator, "Bo"}}};
    sent.level = 3;
    sent.result = GameResult::Victory;
    assert(makeLobbyState(sent) == (std::vector<uint8_t>{19, 'K', '7', 'Q', 'X', 5, 'A', 'l', 'p', 'h', 'a',
                                                         2, 3, 2, 2, 1, 2, 1, 1, 0x70, 0x11, 0x01, 0x00,
                                                         3, 'A', 'n', 'n',
                                                         2, 2, 0, 0, 0, 0, 2, 'B', 'o'}));
    std::optional<Packet> lobby = decode(makeLobbyState(sent));
    assert(lobby && lobby->lobby.code == "K7QX" && lobby->lobby.yourSlot == 2 && lobby->lobby.level == 3 &&
           lobby->lobby.result == GameResult::Victory &&
           lobby->lobby.leaderSlot == 1 && lobby->lobby.members.size() == 2 &&
           lobby->lobby.members[1].role == MemberRole::Spectator && lobby->lobby.members[1].name == "Bo" &&
           lobby->lobby.members[0].score == 70000 && lobby->lobby.members[1].score == 0);
    assert(decode(makeLobbyRename("Beta"))->text == "Beta");
    assert(makeLobbyStart() == std::vector<uint8_t>{21});
    assert(decode(makeLobbyStart())->type == MessageType::LobbyStart);

    // Lobby messages: invalid packets.
    assert(!decode({15, 0}));                     // empty name
    assert(!decode({15, 1, 'A', 0}));             // trailing byte (there is no map)
    assert(!decode({23}));                        // unknown type
    assert(!decode({20, 2, 'A', 0x07}));          // non-printable byte
    assert(!decode({20, 3, 'A', 'B'}));           // length longer than payload
    assert(!decode({16, 'K', '7', 'Q'}));         // short code
    assert(!decode({16, 'k', '7', 'q', 'x'}));    // lowercase is not in the alphabet
    assert(!decode({16, 'O', '0', 'I', '1'}));    // excluded characters
    assert(!decode({14, 33}));                    // more than 32 lobbies
    assert(!decode({17, 4}));                     // unknown refuse reason
    std::vector<uint8_t> badRole = makeLobbyState(sent);
    badRole[badRole.size() - 8] = 3;              // role of the last member (before score and name)
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
