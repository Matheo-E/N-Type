#pragma once

#include <cstdint>

#include <ntype/engine/platform/IInputProvider.hpp>

namespace ntype::client {

/// Samples the keyboard into the protocol's input bitmask.
/// Arrows or ZQSD/WASD to move, Space to fire.
class InputMapper {
public:
    [[nodiscard]] std::uint8_t sample(const engine::platform::IInputProvider& input) const;
};

} // namespace ntype::client
