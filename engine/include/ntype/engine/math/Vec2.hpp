#pragma once

#include <cmath>
#include <cstdint>

namespace ntype::engine::math {

/// Plain 2D vector. Arithmetic is component-wise.
template <typename T>
struct Vec2 {
    T x{};
    T y{};

    constexpr Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }
    constexpr Vec2& operator-=(const Vec2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    constexpr Vec2& operator*=(const T factor) noexcept {
        x *= factor;
        y *= factor;
        return *this;
    }

    friend constexpr Vec2 operator+(Vec2 lhs, const Vec2& rhs) noexcept { return lhs += rhs; }
    friend constexpr Vec2 operator-(Vec2 lhs, const Vec2& rhs) noexcept { return lhs -= rhs; }
    friend constexpr Vec2 operator*(Vec2 lhs, const T factor) noexcept { return lhs *= factor; }
    friend constexpr Vec2 operator*(const T factor, Vec2 rhs) noexcept { return rhs *= factor; }
    friend constexpr bool operator==(const Vec2&, const Vec2&) = default;

    [[nodiscard]] T length() const noexcept { return std::hypot(x, y); }
};

using Vec2f = Vec2<float>;
using Vec2i = Vec2<std::int32_t>;
using Vec2u = Vec2<std::uint32_t>;

/// Linear interpolation between two points, `t` in [0, 1].
[[nodiscard]] constexpr Vec2f lerp(const Vec2f& from, const Vec2f& to, const float t) noexcept {
    return from + (to - from) * t;
}

} // namespace ntype::engine::math
