#pragma once

#include <cstddef>

#include <ntype/engine/platform/IRenderer.hpp>

namespace ntype::engine::platform {

/// Renderer that draws nothing. Used by headless builds and tests; it counts
/// draw calls so tests can check that something was submitted.
class NullRenderer final : public IRenderer {
public:
    explicit NullRenderer(math::Vec2f logical_size = {1280.0F, 720.0F}) noexcept
        : logical_size_(logical_size) {}

    bool load_texture(std::string_view, const std::filesystem::path&) override { return true; }
    bool load_font(std::string_view, const std::filesystem::path&) override { return true; }
    void begin_frame(Color) override { draw_calls_ = 0; }
    void draw_sprite(const SpriteDraw&) override { ++draw_calls_; }
    void draw_rect(const math::Rectf&, Color) override { ++draw_calls_; }
    void draw_text(std::string_view, std::string_view, math::Vec2f, unsigned, Color,
                   TextAlign) override {
        ++draw_calls_;
    }
    void end_frame() override {}
    [[nodiscard]] math::Vec2f logical_size() const override { return logical_size_; }

    [[nodiscard]] std::size_t draw_calls() const noexcept { return draw_calls_; }

private:
    math::Vec2f logical_size_;
    std::size_t draw_calls_ = 0;
};

} // namespace ntype::engine::platform
