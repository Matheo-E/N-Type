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
