#include <ntype/engine/ecs/Registry.hpp>

namespace ntype::engine::ecs {

Entity Registry::create() {
    std::uint32_t index = 0;
    if (!free_list_.empty()) {
        index = free_list_.back();
        free_list_.pop_back();
    } else {
        if (generations_.size() > Entity::kIndexMask) {
            throw std::length_error("too many entities");
        }
        index = static_cast<std::uint32_t>(generations_.size());
        generations_.push_back(0);
    }
    ++alive_count_;
    return Entity::make(index, generations_[index]);
}

void Registry::destroy(const Entity entity) {
    if (!is_alive(entity)) {
        return;
    }
    for (auto& [type, components] : storages_) {
        components->remove(entity);
    }
    const std::uint32_t index = entity.index();
    generations_[index] = (generations_[index] + 1) & Entity::kGenerationMask;
    free_list_.push_back(index);
    --alive_count_;
}

bool Registry::is_alive(const Entity entity) const noexcept {
    const std::uint32_t index = entity.index();
    return !entity.is_null() && index < generations_.size() &&
           generations_[index] == entity.generation();
}

void Registry::clear() {
    for (auto& [type, components] : storages_) {
        components->clear();
    }
    // Bump every generation so handles from before the clear stay stale.
    free_list_.clear();
    for (std::uint32_t index = 0; index < generations_.size(); ++index) {
        generations_[index] = (generations_[index] + 1) & Entity::kGenerationMask;
        free_list_.push_back(index);
    }
    alive_count_ = 0;
}

} // namespace ntype::engine::ecs
