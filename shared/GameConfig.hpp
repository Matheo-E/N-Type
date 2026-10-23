#pragma once

#include <cstddef>
#include <cstdint>

// Game settings both sides must agree on. The wire format itself is in
// common/protocol (see docs/PROTOCOL_v2.md).
namespace ntype::config {

inline constexpr std::uint16_t kDefaultPort = 4242;
inline constexpr std::size_t kMaxPlayers = 4; // Player ids 1 to 4.

// The server clamps positions to this world, the client draws it.
inline constexpr unsigned kWidth = 1280;
inline constexpr unsigned kHeight = 720;
inline constexpr float kPlayerSize = 40.0F;
inline constexpr float kPlayerSpeed = 300.0F; // pixels per second

// Entity kind used for players in WorldSnapshot.
inline constexpr std::uint8_t kPlayerKind = 1;

} // namespace ntype::config
