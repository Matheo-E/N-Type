#pragma once

#include <vector>

#include <ntype/engine/math/Random.hpp>
#include <ntype/engine/math/Vec2.hpp>
#include <ntype/engine/platform/IRenderer.hpp>

namespace ntype::client {

/// Three-layer parallax star-field scrolling right to left.
class Starfield {
public:
    Starfield(engine::math::Vec2f area, std::size_t star_count, std::uint32_t seed);

    void update(float dt);
    void render(engine::platform::IRenderer& renderer) const;

private:
    struct Star {
        engine::math::Vec2f position;
        float speed;
        float size;
        std::uint8_t brightness;
    };

    engine::math::Vec2f area_;
    engine::math::Random rng_;
    std::vector<Star> stars_;
};

} // namespace ntype::client
