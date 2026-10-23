/*
** EPITECH PROJECT, 2026
** N-Type
** File description:
** Protocol
*/

#pragma once
#include "MessageType.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace protocol {

// An object in the game (player, enemy or projectile).
struct Entity {
    uint16_t id = 0;
    uint8_t kind = 0;  // 1=player, 2=enemy, 3=player shot, 4=enemy shot
    int16_t x = 0;
    int16_t y = 0;
};

// A decoded message. The fields used depend on its type.
struct Packet {
    MessageType type = MessageType::ConnectRequest;
    uint16_t id = 0;       // Player ID or entity ID
    uint8_t value = 0;     // Keys, refusal reason, kind, event or game state
    int16_t x = 0;
    int16_t y = 0;
    std::vector<Entity> entities; // Only used for WorldSnapshot
};

std::vector<uint8_t> makeConnectRequest();
std::vector<uint8_t> makeConnectAccept(uint8_t playerId);
std::vector<uint8_t> makeConnectRefuse(RefuseReason reason);
std::vector<uint8_t> makeDisconnect();
std::vector<uint8_t> makeInput(uint8_t keys);
std::vector<uint8_t> makeEntityUpdate(uint16_t id, int16_t x, int16_t y);
std::vector<uint8_t> makeEntitySpawn(uint16_t id, uint8_t kind, int16_t x, int16_t y);
std::vector<uint8_t> makeEntityDestroy(uint16_t id);
std::vector<uint8_t> makeWorldSnapshot(const std::vector<Entity>& entities);
std::vector<uint8_t> makePlayerEvent(uint8_t playerId, uint8_t event);
std::vector<uint8_t> makeClientLeft(uint8_t playerId);
std::vector<uint8_t> makeGameStateChange(uint8_t state);

// Returns std::nullopt if the received message is invalid.
std::optional<Packet> decode(const std::vector<uint8_t>& data);

}
