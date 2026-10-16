#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <ntype/engine/platform/IRenderer.hpp>
#include <ntype/engine/resources/ResourceManager.hpp>

namespace ntype::engine::sfml {

class SfmlWindow;

/// Draws in a fixed logical resolution, letterboxed into the window.
class SfmlRenderer final : public platform::IRenderer {
public:
    SfmlRenderer(SfmlWindow& window, math::Vec2f logical_size);

    bool load_texture(std::string_view id, const std::filesystem::path& path) override;
    bool load_font(std::string_view id, const std::filesystem::path& path) override;

    void begin_frame(platform::Color clear) override;
    void draw_sprite(const platform::SpriteDraw& sprite) override;
    void draw_rect(const math::Rectf& rect, platform::Color color) override;
    void draw_text(std::string_view font, std::string_view text, math::Vec2f position,
                   unsigned size, platform::Color color, platform::TextAlign align) override;
    void end_frame() override;

    [[nodiscard]] math::Vec2f logical_size() const override { return logical_size_; }

    /// Saves the frame being drawn (call before end_frame). Returns false on failure.
    bool save_screenshot(const std::filesystem::path& path);

private:
    void update_view();

    SfmlWindow& window_;
    math::Vec2f logical_size_;
    resources::ResourceManager<sf::Texture> textures_;
    resources::ResourceManager<sf::Font> fonts_;
};

} // namespace ntype::engine::sfml
