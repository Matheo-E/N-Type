#pragma once

#include <ntype/engine/math/Vec2.hpp>

namespace ntype::engine::math {

/// Axis-aligned rectangle given by its top-left corner and its size.
template <typename T>
struct Rect {
    T x{};
    T y{};
    T width{};
    T height{};

    [[nodiscard]] constexpr T left() const noexcept { return x; }
    [[nodiscard]] constexpr T top() const noexcept { return y; }
    [[nodiscard]] constexpr T right() const noexcept { return x + width; }
    [[nodiscard]] constexpr T bottom() const noexcept { return y + height; }

    [[nodiscard]] constexpr bool contains(const Vec2<T>& point) const noexcept {
        return point.x >= left() && point.x < right() && point.y >= top() && point.y < bottom();
    }

    /// True when the two rectangles overlap (touching edges do not count).
    [[nodiscard]] constexpr bool intersects(const Rect& other) const noexcept {
        return left() < other.right() && other.left() < right() && top() < other.bottom() &&
               other.top() < bottom();
    }

    /// Rectangle of the given size centred on `center`.
    [[nodiscard]] static constexpr Rect centered(const Vec2<T>& center,
                                                 const Vec2<T>& size) noexcept {
        return {center.x - size.x / 2, center.y - size.y / 2, size.x, size.y};
    }

    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

using Rectf = Rect<float>;
using Recti = Rect<std::int32_t>;

} // namespace ntype::engine::math
