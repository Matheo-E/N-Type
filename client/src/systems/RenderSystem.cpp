#include "RenderSystem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "../ClientComponents.hpp"

namespace ntype::client {

void RenderSystem::draw(engine::ecs::Registry& registry, const float time) {
    struct Item {
        const rtype::Sprite* sprite;
        rtype::Vec2f position;
        std::uint8_t alpha;
    };
    std::vector<Item> items;
    registry.view<rtype::Sprite, rtype::Transform>().each([&](const engine::ecs::Entity entity,
                                                              const rtype::Sprite& sprite,
                                                              const rtype::Transform& transform) {
        std::uint8_t alpha = 255;
        if (const auto* flags = registry.try_get<ReplicaFlags>(entity)) {
            if (flags->hidden) {
                return;
            }
            // Blink at 2.5 Hz while invulnerable: below the 3 Hz limit
            // for flashing content (accessibility).
            if (flags->invulnerable && std::fmod(time, 0.4F) < 0.2F) {
                alpha = 90;
            }
        }
        items.push_back({&sprite, transform.position, alpha});
    });
    std::ranges::stable_sort(items, {}, [](const Item& item) { return item.sprite->layer; });
    for (const Item& item : items) {
        engine::platform::Color tint = item.sprite->tint;
        tint.a = item.alpha;
        renderer_.draw_sprite({item.sprite->texture,
                               item.sprite->source,
                               item.position,
                               {item.sprite->scale, item.sprite->scale},
                               tint});
    }
}

} // namespace ntype::client
