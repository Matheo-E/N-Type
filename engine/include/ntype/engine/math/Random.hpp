#pragma once

#include <cstdint>

namespace ntype::engine::math {

/// Small seeded pseudo-random generator (xoshiro128**).
///
/// Unlike the standard distributions, its output is identical on every
/// compiler and platform, so a seeded simulation is reproducible.
class Random {
public:
    explicit Random(std::uint32_t seed) noexcept;

    [[nodiscard]] std::uint32_t next_u32() noexcept;
    /// Uniform float in [min, max).
    [[nodiscard]] float next_float(float min, float max) noexcept;
    /// Uniform integer in [min, max] (both inclusive).
    [[nodiscard]] std::int32_t next_int(std::int32_t min, std::int32_t max) noexcept;

private:
    std::uint32_t state_[4]{};
};

} // namespace ntype::engine::math
