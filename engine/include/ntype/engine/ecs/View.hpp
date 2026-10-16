#pragma once

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <tuple>
#include <vector>

#include <ntype/engine/ecs/ComponentStorage.hpp>

namespace ntype::engine::ecs {

/// Iterates every entity that has all of `Ts...`.
///
/// The entity list is copied before iterating, so the callback may destroy
/// entities or add/remove components. It must not keep component references
/// across such a change.
template <typename... Ts>
class View {
public:
    explicit View(ComponentStorage<Ts>*... storages) noexcept : storages_{storages...} {}

    /// Calls `callback(Entity, Ts&...)` for every matching entity.
    template <typename Callback>
    void each(Callback&& callback) {
        for (const Entity entity : entities()) {
            auto components =
                std::make_tuple(std::get<ComponentStorage<Ts>*>(storages_)->try_get(entity)...);
            const bool complete =
                std::apply([](auto*... ptrs) { return ((ptrs != nullptr) && ...); }, components);
            if (complete) {
                std::apply([&](auto*... ptrs) { callback(entity, *ptrs...); }, components);
            }
        }
    }

    /// Snapshot of the entities matching the view at call time.
    [[nodiscard]] std::vector<Entity> entities() const {
        const bool any_missing = ((std::get<ComponentStorage<Ts>*>(storages_) == nullptr) || ...);
        if (any_missing) {
            return {};
        }
        // Walk the smallest storage: every match must be in it.
        const std::array<std::span<const Entity>, sizeof...(Ts)> candidates{
            std::get<ComponentStorage<Ts>*>(storages_)->entities()...};
        const auto smallest = std::ranges::min_element(
            candidates, {}, [](const std::span<const Entity> span) { return span.size(); });
        std::vector<Entity> result;
        result.reserve(smallest->size());
        std::ranges::copy_if(*smallest, std::back_inserter(result), [this](const Entity entity) {
            return (std::get<ComponentStorage<Ts>*>(storages_)->contains(entity) && ...);
        });
        return result;
    }

private:
    std::tuple<ComponentStorage<Ts>*...> storages_;
};

} // namespace ntype::engine::ecs
