#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <ntype/engine/ecs/ISystem.hpp>

namespace ntype::engine::ecs {

/// Runs systems in the order they were added.
class SystemScheduler {
public:
    template <typename S, typename... Args>
    S& add(Args&&... args) {
        auto system = std::make_unique<S>(std::forward<Args>(args)...);
        S& reference = *system;
        systems_.push_back(std::move(system));
        return reference;
    }

    void update(Registry& registry, float dt);
    void clear() noexcept { systems_.clear(); }
    [[nodiscard]] std::size_t size() const noexcept { return systems_.size(); }

private:
    std::vector<std::unique_ptr<ISystem>> systems_;
};

} // namespace ntype::engine::ecs
