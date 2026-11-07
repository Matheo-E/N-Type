/*
** EPITECH PROJECT, 2026
** N-Type
** File description:
** MessageType
*/

#pragma once
#include <cstdint>

namespace protocol {

enum class MessageType : uint8_t {
    ConnectRequest = 1,
    ConnectAccept = 2,
    ConnectRefuse = 3,
    Disconnect = 4,
    Input = 5,
    EntityUpdate = 6,
    EntitySpawn = 7,
    EntityDestroy = 8,
    WorldSnapshot = 9,
    PlayerEvent = 10,
    ClientLeft = 11,
    GameStateChange = 12,
    LobbyListRequest = 13,
    LobbyList = 14,
    LobbyCreate = 15,
    LobbyJoin = 16,
    LobbyRefuse = 17,
    LobbyLeave = 18,
    LobbyState = 19,
    LobbyRename = 20,
    LobbyStart = 21,
    EntityDied = 22,
};

// What an entity is. Its `type` then says which one: the enemy type in
// enemies.json, the attack pattern of a shot, or the item type.
enum class EntityKind : uint8_t {
    Player = 1,
    Enemy = 2,
    PlayerShot = 3,
    EnemyShot = 4,
    Item = 5,
};

enum class RefuseReason : uint8_t {
    ServerFull = 1,
    ServerError = 2
};

enum class LobbyRefuseReason : uint8_t {
    NotFound = 1,
    Full = 2,
    TooManyLobbies = 3
};

enum class LobbyPhase : uint8_t {
    Waiting = 1,
    Playing = 2
};

// How the lobby's last game ended, shown on the results screen.
enum class GameResult : uint8_t {
    None = 0,    // no game ended yet, or a new one is running
    Defeat = 1,  // every player died (game over)
    Victory = 2  // the last boss died
};

enum class MemberRole : uint8_t {
    Player = 1,
    Spectator = 2
};

enum class InputType : uint8_t {
    Up = 1,
    Down = 2,
    Left = 4,
    Right = 8,
    Shoot = 16,
};

}
