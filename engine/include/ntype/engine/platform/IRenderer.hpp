#pragma once

#include <filesystem>
#include <string_view>

#include <ntype/engine/math/Rect.hpp>
#include <ntype/engine/math/Vec2.hpp>
#include <ntype/engine/platform/Color.hpp>

namespace ntype::engine::platform {

/// Parameters of one sprite draw. `position` is the sprite centre in world units.
struct SpriteDraw {
    std::string_view texture;
    math::Recti source;
    math::Vec2f position;
    math::Vec2f scale{1.0F, 1.0F};
    Color tint = colors::kWhite;
};

enum class TextAlign { Left, Center };

/// 2D renderer working in a fixed logical resolution; the backend scales it
/// to the real window size.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// Returns false when the file cannot be loaded.
    virtual bool load_texture(std::string_view id, const std::filesystem::path& path) = 0;
    virtual bool load_font(std::string_view id, const std::filesystem::path& path) = 0;

    virtual void begin_frame(Color clear) = 0;
    virtual void draw_sprite(const SpriteDraw& sprite) = 0;
    virtual void draw_rect(const math::Rectf& rect, Color color) = 0;
    virtual void draw_text(std::string_view font, std::string_view text, math::Vec2f position,
                           unsigned size, Color color, TextAlign align) = 0;
    virtual void end_frame() = 0;

    [[nodiscard]] virtual math::Vec2f logical_size() const = 0;
};

} // namespace ntype::engine::platform
