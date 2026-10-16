#include "Starfield.hpp"

namespace ntype::client {

namespace {

struct Layer {
    float speed;
    float size;
    std::uint8_t brightness;
};

constexpr Layer kLayers[] = {{30.0F, 1.0F, 90}, {80.0F, 2.0F, 160}, {180.0F, 3.0F, 240}};

} // namespace

Starfield::Starfield(const engine::math::Vec2f area, const std::size_t star_count,
                     const std::uint32_t seed)
    : area_(area), rng_(seed) {
    stars_.reserve(star_count);
    for (std::size_t i = 0; i < star_count; ++i) {
        const Layer& layer = kLayers[i % std::size(kLayers)];
        stars_.push_back({{rng_.next_float(0.0F, area.x), rng_.next_float(0.0F, area.y)},
                          layer.speed * rng_.next_float(0.8F, 1.2F),
                          layer.size,
                          layer.brightness});
    }
}

void Starfield::update(const float dt) {
    for (auto& star : stars_) {
        star.position.x -= star.speed * dt;
        if (star.position.x < -star.size) {
            star.position = {area_.x + rng_.next_float(0.0F, 40.0F),
                             rng_.next_float(0.0F, area_.y)};
        }
    }
}

void Starfield::render(engine::platform::IRenderer& renderer) const {
    for (const auto& star : stars_) {
        const std::uint8_t level = star.brightness;
        renderer.draw_rect({star.position.x, star.position.y, star.size, star.size},
                           {level, level, static_cast<std::uint8_t>(level / 2 + 127), 255});
    }
}

} // namespace ntype::client
