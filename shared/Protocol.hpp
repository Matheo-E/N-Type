#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ntype::protocol {

constexpr unsigned kWidth = 800;
constexpr unsigned kHeight = 600;
constexpr float kSquareSize = 40.0F;
constexpr float kSpeed = 250.0F;
constexpr std::uint16_t kDefaultPort = 4242;
constexpr std::uint8_t kLeft = 1;
constexpr std::uint8_t kRight = 2;
constexpr std::uint8_t kUp = 4;
constexpr std::uint8_t kDown = 8;
constexpr std::uint8_t kAllInputs = 15;

enum class Type : std::uint8_t { Connect = 1, Input, State, Disconnect, Full };

struct Position {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
};

// A message has a connection nonce, then fields selected by its type.
struct Message {
    Type type = Type::Connect;
    std::uint32_t nonce = 0;
    std::uint32_t sequence = 0;
    std::uint8_t slot = 0;
    std::uint8_t bits = 0;
    std::array<Position, 2> positions{};
};

// Unsigned subtraction handles sequence wrap-around (within half the range).
inline bool newer(std::uint32_t candidate, std::uint32_t previous) {
    const auto distance = candidate - previous;
    return distance != 0 && distance < 0x80000000U;
}

inline std::vector<std::uint8_t> encode(const Message& message) {
    std::vector<std::uint8_t> bytes{'N', 'T', 1, static_cast<std::uint8_t>(message.type)};
    const auto append = [&](std::uint32_t value, unsigned size) {
        for (unsigned index = 0; index < size; ++index) {
            bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU));
        }
    };
    append(message.nonce, 4);
    if (message.type == Type::Input || message.type == Type::State) {
        append(message.sequence, 4);
    }
    if (message.type == Type::Input) {
        append(message.bits, 1);
    } else if (message.type == Type::State) {
        append(message.slot, 1);
        append(message.bits, 1); // Connected player mask, one bit per slot.
        for (const auto& position : message.positions) {
            append(position.x, 2);
            append(position.y, 2);
        }
    }
    return bytes;
}

inline std::optional<Message> decode(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 8 || bytes[0] != 'N' || bytes[1] != 'T' || bytes[2] != 1 ||
        bytes[3] < 1 || bytes[3] > 5) {
        return std::nullopt;
    }
    Message message;
    message.type = static_cast<Type>(bytes[3]);
    const std::size_t expected = message.type == Type::Input ? 13 :
                                 message.type == Type::State ? 22 : 8;
    if (bytes.size() != expected) {
        return std::nullopt;
    }
    std::size_t offset = 4;
    const auto read = [&](unsigned size) {
        std::uint32_t value = 0;
        for (unsigned index = 0; index < size; ++index) {
            value |= static_cast<std::uint32_t>(bytes[offset++]) << (index * 8);
        }
        return value;
    };
    message.nonce = read(4);
    if (message.type == Type::Input || message.type == Type::State) {
        message.sequence = read(4);
    }
    if (message.type == Type::Input) {
        message.bits = static_cast<std::uint8_t>(read(1));
        if ((message.bits & ~kAllInputs) != 0) {
            return std::nullopt;
        }
    } else if (message.type == Type::State) {
        message.slot = static_cast<std::uint8_t>(read(1));
        message.bits = static_cast<std::uint8_t>(read(1));
        if (message.slot >= 2 || message.bits > 3 ||
            (message.bits & (1U << message.slot)) == 0) {
            return std::nullopt;
        }
        for (auto& position : message.positions) {
            position.x = static_cast<std::uint16_t>(read(2));
            position.y = static_cast<std::uint16_t>(read(2));
            if (position.x > kWidth - 40 || position.y > kHeight - 40) {
                return std::nullopt;
            }
        }
    }
    return message;
}

} // namespace ntype::protocol
