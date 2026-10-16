#pragma once

#include <string_view>

namespace ntype::engine::ecs {

class Registry;

/// One step of the world update. Systems never call each other: they share
/// data through components and communicate through the EventBus.
class ISystem {
public:
    virtual ~ISystem() = default;

    virtual void update(Registry& registry, float dt) = 0;
    [[nodiscard]] virtual std::string_view name() const = 0;
};

} // namespace ntype::engine::ecs
