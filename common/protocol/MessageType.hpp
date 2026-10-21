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
};

enum class RefuseReason : uint8_t {
    ServerFull = 1,
    ServerError = 2
};

enum class InputType : uint8_t {
    Up = 1,
    Down = 2,
    Left = 4,
    Right = 8,
    Shoot = 16,
};

}
