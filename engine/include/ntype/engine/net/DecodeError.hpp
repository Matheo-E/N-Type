#pragma once

#include <cstdint>
#include <string_view>

namespace ntype::engine::net {

/// Why a datagram or a message was rejected. Malformed input from the
/// network is an expected event, so it is reported as a value, never thrown.
enum class DecodeError : std::uint8_t {
    Truncated,        ///< fewer bytes than the layout requires
    BadMagic,         ///< not one of our packets
    BadVersion,       ///< protocol version mismatch
    OversizedPayload, ///< declared payload larger than allowed
    TrailingData,     ///< more bytes than the header declared
    UnknownType,      ///< message type not part of the protocol
    InvalidValue,     ///< a field holds a value outside its range
};

[[nodiscard]] constexpr std::string_view to_string(const DecodeError error) noexcept {
    switch (error) {
    case DecodeError::Truncated:
        return "truncated";
    case DecodeError::BadMagic:
        return "bad magic";
    case DecodeError::BadVersion:
        return "bad version";
    case DecodeError::OversizedPayload:
        return "oversized payload";
    case DecodeError::TrailingData:
        return "trailing data";
    case DecodeError::UnknownType:
        return "unknown type";
    case DecodeError::InvalidValue:
        return "invalid value";
    }
    return "unknown error";
}

} // namespace ntype::engine::net
