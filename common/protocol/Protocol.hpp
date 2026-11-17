/*
** EPITECH PROJECT, 2026
** N-Type
** File description:
** Protocol
*/

#pragma once
#include "MessageType.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace protocol {
inline constexpr uint8_t kGoldenPlayer = 1; // player type while the golden shield is active

// Limits checked by the encoder and the decoder.
inline constexpr std::size_t kMaxPlayerName = 16;
inline constexpr std::size_t kMaxLobbyName = 24;
inline constexpr std::size_t kCodeLength = 4;
inline constexpr std::size_t kMaxLobbies = 32;
inline constexpr std::size_t kMaxMembers = 4;
// A frame of the game is sent in parts, each one small enough for any network.
inline constexpr std::size_t kMaxSnapshotEntities = 90; // per part: 8 + 90 x 14 = 1268 bytes
inline constexpr std::size_t kMaxSnapshotParts = 32;
// Room code characters: no O/0 or I/1, which are easy to confuse.
inline constexpr std::string_view kCodeAlphabet = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

// An object in the game. Players have ids 1 to 4 (their lobby slot) and their x, y
// are the top-left corner of their 40x40 box; other entities have ids from 5 and
// their x, y are their centre.
struct Entity {
    uint16_t id = 0;
    uint8_t kind = 0;  // EntityKind: 1=player, 2=enemy, 3=player shot, 4=enemy shot, 5=item
    int32_t x = 0;
    int32_t y = 0;
    uint8_t type = 0;  // enemy/shot/item type; player: 0 normal, kGoldenPlayer shield
    uint8_t hp = 0;    // health percent; target zones: charge 0..99, active 100; otherwise 0
    uint8_t hits = 0;  // counts every hit it took, wrapping at 256: a change means "just hit"
};

// One row of the lobby list.
struct LobbyInfo {
    std::string code;
    std::string name;
    uint8_t members = 0;
    LobbyPhase state = LobbyPhase::Waiting;
};

struct LobbyMember {
    uint8_t slot = 0; // 1 to 4, also the player's entity id in game
    MemberRole role = MemberRole::Player;
    std::string name;
    uint32_t score = 0; // points in the current (or last) game
};

// The lobby as seen by one member (yourSlot is that member).
struct LobbyState {
    std::string code;
    std::string name;
    LobbyPhase state = LobbyPhase::Waiting;
    uint8_t yourSlot = 0;
    uint8_t leaderSlot = 0;
    std::vector<LobbyMember> members;
    uint8_t level = 0; // level being played, or reached by the last game, from 1; 0 before any game
    GameResult result = GameResult::None; // how the last game ended; None while one runs
};

// A decoded message. The fields used depend on its type.
struct Packet {
    MessageType type = MessageType::ConnectRequest;
    uint16_t id = 0;       // Player ID or entity ID
    uint8_t value = 0;     // Keys, refusal reason, kind, event or game state
    int32_t x = 0;
    int32_t y = 0;
    std::vector<Entity> entities; // WorldSnapshot (this part), or the one EntityDied
    uint32_t frame = 0;           // WorldSnapshot: which frame this part belongs to
    uint8_t part = 0;             // WorldSnapshot: index of this part, from 0
    uint8_t parts = 0;            // WorldSnapshot: how many parts the frame has
    std::string text;             // Player or lobby name
    std::string code;             // Room code (LobbyJoin)
    std::vector<LobbyInfo> lobbies; // Only used for LobbyList
    LobbyState lobby;             // Only used for LobbyState
};

std::vector<uint8_t> makeConnectRequest(const std::string& name);
std::vector<uint8_t> makeConnectAccept();
std::vector<uint8_t> makeConnectRefuse(RefuseReason reason);
std::vector<uint8_t> makeDisconnect();
std::vector<uint8_t> makeInput(uint8_t keys);
std::vector<uint8_t> makeEntityUpdate(uint16_t id, int32_t x, int32_t y);
std::vector<uint8_t> makeEntitySpawn(uint16_t id, uint8_t kind, int32_t x, int32_t y);
std::vector<uint8_t> makeEntityDestroy(uint16_t id);
// One part of a frame: at most kMaxSnapshotEntities entities.
std::vector<uint8_t> makeWorldSnapshot(uint32_t frame, uint8_t part, uint8_t parts,
                                       const std::vector<Entity>& entities);
// A whole frame, split into as many parts as needed (one empty part if there is nothing).
std::vector<std::vector<uint8_t>> makeWorldSnapshots(uint32_t frame, const std::vector<Entity>& entities);
std::vector<uint8_t> makeEntityDied(const Entity& entity);
std::vector<uint8_t> makePlayerEvent(uint8_t playerId, uint8_t event);
std::vector<uint8_t> makeClientLeft(uint8_t playerId);
std::vector<uint8_t> makeGameStateChange(uint8_t state);
std::vector<uint8_t> makeLobbyListRequest();
std::vector<uint8_t> makeLobbyList(const std::vector<LobbyInfo>& lobbies);
std::vector<uint8_t> makeLobbyCreate(const std::string& name);
std::vector<uint8_t> makeLobbyJoin(const std::string& code);
std::vector<uint8_t> makeLobbyRefuse(LobbyRefuseReason reason);
std::vector<uint8_t> makeLobbyLeave();
std::vector<uint8_t> makeLobbyState(const LobbyState& lobby);
std::vector<uint8_t> makeLobbyRename(const std::string& name);
std::vector<uint8_t> makeLobbyStart();

// Returns std::nullopt if the received message is invalid.
std::optional<Packet> decode(const std::vector<uint8_t>& data);

}
