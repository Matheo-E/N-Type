#include <ntype/engine/ecs/SystemScheduler.hpp>

namespace ntype::engine::ecs {

void SystemScheduler::update(Registry& registry, const float dt) {
    for (const auto& system : systems_) {
        system->update(registry, dt);
    }
}

} // namespace ntype::engine::ecs
