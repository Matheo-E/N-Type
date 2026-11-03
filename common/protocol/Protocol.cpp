/*
** EPITECH PROJECT, 2026
** N-Type
** File description:
** Protocol
*/

#include "Protocol.hpp"
#include <stdexcept>

namespace protocol {

static uint8_t messageId(MessageType type)
{
    return static_cast<uint8_t>(type);
}

// A uint16_t occupies two bytes, always in the same order (little-endian).
static void write16(std::vector<uint8_t>& data, uint16_t number)
{
    data.push_back(static_cast<uint8_t>(number & 0xFF));
    data.push_back(static_cast<uint8_t>(number >> 8));
}

static uint16_t read16(const std::vector<uint8_t>& data, size_t position)
{
    return static_cast<uint16_t>(data[position]) |
           (static_cast<uint16_t>(data[position + 1]) << 8);
}

static void write32(std::vector<uint8_t>& data, const int32_t number)
{
    const auto bits = static_cast<uint32_t>(number);
    data.push_back(static_cast<uint8_t>(bits & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((bits >> 24) & 0xFF));
}

static int32_t readPosition(const std::vector<uint8_t>& data, const size_t position)
{
    const uint32_t bits = static_cast<uint32_t>(data[position]) |
        (static_cast<uint32_t>(data[position + 1]) << 8) |
        (static_cast<uint32_t>(data[position + 2]) << 16) |
        (static_cast<uint32_t>(data[position + 3]) << 24);
    return static_cast<int32_t>(bits);
}

std::vector<uint8_t> makeConnectRequest()
{
    return {messageId(MessageType::ConnectRequest)};
}

std::vector<uint8_t> makeConnectAccept(uint8_t playerId)
{
    return {messageId(MessageType::ConnectAccept), playerId};
}

std::vector<uint8_t> makeConnectRefuse(RefuseReason reason)
{
    return {messageId(MessageType::ConnectRefuse), static_cast<uint8_t>(reason)};
}

std::vector<uint8_t> makeDisconnect()
{
    return {messageId(MessageType::Disconnect)};
}

std::vector<uint8_t> makeInput(uint8_t keys)
{
    return {messageId(MessageType::Input), keys};
}

std::vector<uint8_t> makeEntityUpdate(uint16_t id, int32_t x, int32_t y)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntityUpdate)};
    write16(data, id);
    write32(data, x);
    write32(data, y);
    return data;
}

std::vector<uint8_t> makeEntitySpawn(uint16_t id, uint8_t kind, int32_t x, int32_t y)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntitySpawn)};
    write16(data, id);
    data.push_back(kind);
    write32(data, x);
    write32(data, y);
    return data;
}

std::vector<uint8_t> makeEntityDestroy(uint16_t id)
{
    std::vector<uint8_t> data = {messageId(MessageType::EntityDestroy)};
    write16(data, id);
    return data;
}

std::vector<uint8_t> makeWorldSnapshot(const std::vector<Entity>& entities)
{
    // Keep one datagram reasonably small for this MVP.
    if (entities.size() > 100)
        throw std::invalid_argument("Too many entities in snapshot");

    std::vector<uint8_t> data = {messageId(MessageType::WorldSnapshot), static_cast<uint8_t>(entities.size())};
    for (const Entity& entity : entities) {
        write16(data, entity.id);
        data.push_back(entity.kind);
        write32(data, entity.x);
        write32(data, entity.y);
    }
    return data;
}

std::vector<uint8_t> makePlayerEvent(uint8_t playerId, uint8_t event)
{
    return {messageId(MessageType::PlayerEvent), playerId, event};
}

std::vector<uint8_t> makeClientLeft(uint8_t playerId)
{
    return {messageId(MessageType::ClientLeft), playerId};
}

std::vector<uint8_t> makeGameStateChange(uint8_t state)
{
    return {messageId(MessageType::GameStateChange), state};
}

std::optional<Packet> decode(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return std::nullopt;

    Packet packet;
    packet.type = static_cast<MessageType>(data[0]);

    switch (packet.type) {
        case MessageType::ConnectRequest:
        case MessageType::Disconnect:
            if (data.size() != 1)
                return std::nullopt;
            break;

        case MessageType::ConnectAccept:
            if (data.size() != 2 || data[1] < 1 || data[1] > 4)
                return std::nullopt;
            packet.id = data[1];
            break;

        case MessageType::ConnectRefuse:
            if (data.size() != 2 || data[1] < 1 || data[1] > 2)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::Input:
            if (data.size() != 2 || (data[1] & 0xE0) != 0)
                return std::nullopt;
            packet.value = data[1];
            break;

        case MessageType::EntityUpdate:
            if (data.size() != 11)
                return std::nullopt;
            packet.id = read16(data, 1);
            packet.x = readPosition(data, 3);
            packet.y = readPosition(data, 7);
            break;

        case MessageType::EntitySpawn:
            if (data.size() != 12 || data[3] < 1 || data[3] > 4)
                return std::nullopt;
            packet.id = read16(data, 1);
            packet.value = data[3];
            packet.x = readPosition(data, 4);
            packet.y = readPosition(data, 8);
            break;

        case MessageType::EntityDestroy:
            if (data.size() != 3)
                return std::nullopt;
            packet.id = read16(data, 1);
            break;

        case MessageType::WorldSnapshot: {
            if (data.size() < 2 || data[1] > 100 ||
                data.size() != 2 + static_cast<size_t>(data[1]) * 11)
                return std::nullopt;
            for (size_t i = 0; i < data[1]; i++) {
                size_t pos = 2 + i * 11;
                if (data[pos + 2] < 1 || data[pos + 2] > 4)
                    return std::nullopt;
                Entity entity;
                entity.id = read16(data, pos);
                entity.kind = data[pos + 2];
                entity.x = readPosition(data, pos + 3);
                entity.y = readPosition(data, pos + 7);
                packet.entities.push_back(entity);
            }
            break;
        }

        case MessageType::PlayerEvent:
            if (data.size() != 3 || data[1] < 1 || data[1] > 4 ||
                data[2] < 1 || data[2] > 3)
                return std::nullopt;
            packet.id = data[1];
            packet.value = data[2];
            break;

        case MessageType::ClientLeft:
            if (data.size() != 2 || data[1] < 1 || data[1] > 4)
                return std::nullopt;
            packet.id = data[1];
            break;

        case MessageType::GameStateChange:
            if (data.size() != 2 || data[1] < 1 || data[1] > 3)
                return std::nullopt;
            packet.value = data[1];
            break;

        default:
            return std::nullopt;
    }

    return packet;
}

}
