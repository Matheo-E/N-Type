#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ntype/engine/ecs/ComponentStorage.hpp>
#include <ntype/engine/ecs/Entity.hpp>
#include <ntype/engine/ecs/View.hpp>

namespace ntype::engine::ecs {

/// Owns every entity and component of a world.
class Registry {
public:
    [[nodiscard]] Entity create();
    /// Removes all components and recycles the slot. Stale handles are ignored.
    void destroy(Entity entity);
    [[nodiscard]] bool is_alive(Entity entity) const noexcept;
    [[nodiscard]] std::size_t alive_count() const noexcept { return alive_count_; }
    /// Destroys every entity and resets the registry.
    void clear();

    /// Adds the component, or replaces it if the entity already has one.
    template <typename T, typename... Args>
    T& emplace(const Entity entity, Args&&... args) {
        if (!is_alive(entity)) {
            throw std::invalid_argument("emplace on a dead entity");
        }
        return storage<T>().emplace(entity, std::forward<Args>(args)...);
    }

    /// Throws std::out_of_range when the component is missing.
    template <typename T>
    [[nodiscard]] T& get(const Entity entity) {
        return storage<T>().get(entity);
    }

    template <typename T>
    [[nodiscard]] T* try_get(const Entity entity) noexcept {
        auto* found = find_storage<T>();
        return found == nullptr ? nullptr : found->try_get(entity);
    }

    template <typename T>
    [[nodiscard]] const T* try_get(const Entity entity) const noexcept {
        const auto* found = find_storage<T>();
        return found == nullptr ? nullptr : found->try_get(entity);
    }

    template <typename T>
    [[nodiscard]] bool has(const Entity entity) const noexcept {
        return try_get<T>(entity) != nullptr;
    }

    template <typename T>
    void remove(const Entity entity) {
        if (auto* found = find_storage<T>()) {
            found->remove(entity);
        }
    }

    template <typename... Ts>
    [[nodiscard]] View<Ts...> view() {
        return View<Ts...>{find_storage<Ts>()...};
    }

private:
    template <typename T>
    ComponentStorage<T>& storage() {
        auto& slot = storages_[std::type_index(typeid(T))];
        if (!slot) {
            slot = std::make_unique<ComponentStorage<T>>();
        }
        return static_cast<ComponentStorage<T>&>(*slot);
    }

    template <typename T>
    ComponentStorage<T>* find_storage() const noexcept {
        const auto it = storages_.find(std::type_index(typeid(T)));
        return it == storages_.end() ? nullptr
                                     : static_cast<ComponentStorage<T>*>(it->second.get());
    }

    std::vector<std::uint32_t> generations_;
    std::vector<std::uint32_t> free_list_;
    std::size_t alive_count_ = 0;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> storages_;
};

} // namespace ntype::engine::ecs
