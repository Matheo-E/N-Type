#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ntype::engine::ecs {

/// Handle to an entity: a slot index plus a generation counter.
///
/// When an entity is destroyed its slot is recycled with a higher generation,
/// so a handle kept by mistake no longer matches and is detected as stale.
struct Entity {
    static constexpr std::uint32_t kIndexBits = 20;
    static constexpr std::uint32_t kIndexMask = (1U << kIndexBits) - 1U;
    static constexpr std::uint32_t kGenerationMask = (1U << (32U - kIndexBits)) - 1U;
    static constexpr std::uint32_t kNullId = 0xFFFFFFFFU;

    std::uint32_t id = kNullId;

    [[nodiscard]] static constexpr Entity make(const std::uint32_t index,
                                               const std::uint32_t generation) noexcept {
        return Entity{((generation & kGenerationMask) << kIndexBits) | (index & kIndexMask)};
    }

    [[nodiscard]] constexpr std::uint32_t index() const noexcept { return id & kIndexMask; }
    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return (id >> kIndexBits) & kGenerationMask;
    }
    [[nodiscard]] constexpr bool is_null() const noexcept { return id == kNullId; }

    friend constexpr bool operator==(Entity, Entity) = default;
};

inline constexpr Entity kNullEntity{};

} // namespace ntype::engine::ecs

template <>
struct std::hash<ntype::engine::ecs::Entity> {
    std::size_t operator()(const ntype::engine::ecs::Entity entity) const noexcept {
        return std::hash<std::uint32_t>{}(entity.id);
    }
};
