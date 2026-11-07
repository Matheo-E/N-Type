#pragma once

#include <cmath>

#include "SFML/System.hpp"

inline constexpr float Pi = 3.14159265f;

inline float Length(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// The unit vector for an angle in degrees: 0 points down, 90 right, 180 up, -90 left.
inline sf::Vector2f Direction(float degrees) {
    const float radians = degrees * Pi / 180.0f;
    return {std::sin(radians), std::cos(radians)};
}

// Turns a vector by an angle in degrees, in the same convention as Direction:
// Rotate({0, 1}, a) == Direction(a), so a vector pointing down ends up pointing along a.
inline sf::Vector2f Rotate(sf::Vector2f v, float degrees) {
    const float radians = degrees * Pi / 180.0f;
    const float s = std::sin(radians);
    const float c = std::cos(radians);
    return {v.x * c + v.y * s, -v.x * s + v.y * c};
}

// The angle (same convention) of the direction from one point to another.
inline float AngleTo(sf::Vector2f from, sf::Vector2f to) {
    const sf::Vector2f delta = to - from;
    return std::atan2(delta.x, delta.y) * 180.0f / Pi;
}
