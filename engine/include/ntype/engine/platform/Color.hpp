#pragma once

#include <cstdint>

namespace ntype::engine::platform {

struct Color {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;

    friend constexpr bool operator==(const Color&, const Color&) = default;
};

namespace colors {
inline constexpr Color kWhite{255, 255, 255, 255};
inline constexpr Color kBlack{0, 0, 0, 255};
inline constexpr Color kTransparent{0, 0, 0, 0};
} // namespace colors

} // namespace ntype::engine::platform
