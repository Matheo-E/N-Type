#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <ntype/engine/ecs/Entity.hpp>

namespace ntype::engine::ecs {

/// Type-erased side of a component storage, so the registry can remove the
/// components of a destroyed entity without knowing their types.
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    virtual void remove(Entity entity) = 0;
    [[nodiscard]] virtual bool contains(Entity entity) const = 0;
    virtual void clear() = 0;
};

/// Sparse set: O(1) add/remove/lookup, components packed contiguously.
template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
    template <typename... Args>
    T& emplace(const Entity entity, Args&&... args) {
        if (const auto position = find(entity); position != kAbsent) {
            components_[position] = T{std::forward<Args>(args)...};
            return components_[position];
        }
        const std::uint32_t index = entity.index();
        if (index >= sparse_.size()) {
            sparse_.resize(static_cast<std::size_t>(index) + 1, kAbsent);
        }
        sparse_[index] = static_cast<std::uint32_t>(dense_.size());
        dense_.push_back(entity);
        components_.push_back(T{std::forward<Args>(args)...});
        return components_.back();
    }

    [[nodiscard]] T& get(const Entity entity) {
        if (T* component = try_get(entity)) {
            return *component;
        }
        throw std::out_of_range("entity has no such component");
    }

    [[nodiscard]] T* try_get(const Entity entity) noexcept {
        const auto position = find(entity);
        return position == kAbsent ? nullptr : &components_[position];
    }

    [[nodiscard]] const T* try_get(const Entity entity) const noexcept {
        const auto position = find(entity);
        return position == kAbsent ? nullptr : &components_[position];
    }

    void remove(const Entity entity) override {
        const auto position = find(entity);
        if (position == kAbsent) {
            return;
        }
        // Swap with the last element to keep the arrays packed.
        const Entity last = dense_.back();
        dense_[position] = last;
        components_[position] = std::move(components_.back());
        sparse_[last.index()] = position;
        dense_.pop_back();
        components_.pop_back();
        sparse_[entity.index()] = kAbsent;
    }

    [[nodiscard]] bool contains(const Entity entity) const override {
        return find(entity) != kAbsent;
    }

    void clear() override {
        sparse_.clear();
        dense_.clear();
        components_.clear();
    }

    [[nodiscard]] std::size_t size() const noexcept { return dense_.size(); }
    [[nodiscard]] std::span<const Entity> entities() const noexcept { return dense_; }

private:
    static constexpr std::uint32_t kAbsent = std::numeric_limits<std::uint32_t>::max();

    [[nodiscard]] std::uint32_t find(const Entity entity) const noexcept {
        const std::uint32_t index = entity.index();
        if (entity.is_null() || index >= sparse_.size()) {
            return kAbsent;
        }
        const std::uint32_t position = sparse_[index];
        // The generation check rejects stale handles to a recycled slot.
        if (position == kAbsent || dense_[position] != entity) {
            return kAbsent;
        }
        return position;
    }

    std::vector<std::uint32_t> sparse_;
    std::vector<Entity> dense_;
    std::vector<T> components_;
};

} // namespace ntype::engine::ecs
