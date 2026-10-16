#include <bit>

#include <ntype/engine/math/Random.hpp>

namespace ntype::engine::math {

namespace {

std::uint32_t splitmix32(std::uint32_t& x) noexcept {
    std::uint32_t z = (x += 0x9E3779B9U);
    z = (z ^ (z >> 16U)) * 0x85EBCA6BU;
    z = (z ^ (z >> 13U)) * 0xC2B2AE35U;
    return z ^ (z >> 16U);
}

} // namespace

Random::Random(std::uint32_t seed) noexcept {
    for (auto& word : state_) {
        word = splitmix32(seed);
    }
}

std::uint32_t Random::next_u32() noexcept {
    const std::uint32_t result = std::rotl(state_[1] * 5U, 7) * 9U;
    const std::uint32_t t = state_[1] << 9U;
    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = std::rotl(state_[3], 11);
    return result;
}

float Random::next_float(const float min, const float max) noexcept {
    // 24 random bits give every representable float in [0, 1) the same weight.
    const float unit = static_cast<float>(next_u32() >> 8U) * (1.0F / 16777216.0F);
    return min + (max - min) * unit;
}

std::int32_t Random::next_int(const std::int32_t min, const std::int32_t max) noexcept {
    if (max <= min) {
        return min;
    }
    const auto span = static_cast<std::uint32_t>(static_cast<std::int64_t>(max) - min + 1);
    return static_cast<std::int32_t>(min + static_cast<std::int64_t>(next_u32() % span));
}

} // namespace ntype::engine::math
