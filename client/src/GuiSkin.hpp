#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <SFML/Graphics.hpp>

namespace ntype::client {
namespace style {
inline const sf::Color ink{85, 49, 39};
inline const sf::Color muted{133, 95, 66};
} // namespace style

// Rasterize glyphs above their displayed pixel size, then downsample them.
// This keeps text smooth when a logical UI view is enlarged or resized.
inline sf::Text make_ui_text(const sf::RenderWindow& window, const sf::Font& font,
                             const std::string& value, unsigned size) {
    const float pixels_per_unit = static_cast<float>(window.getSize().y) *
        window.getView().getViewport().height / window.getView().getSize().y;
    const unsigned density = static_cast<unsigned>(std::max(2.f, std::ceil(pixels_per_unit * 2.f)));
    sf::Text text(value, font, size * density);
    const float scale = 1.f / static_cast<float>(density);
    text.setScale(scale, scale);
    return text;
}

inline void position_ui_text(const sf::RenderWindow& window, sf::Text& text, float x, float y) {
    // Align the text origin to an actual window pixel.
    text.setPosition(window.mapPixelToCoords(window.mapCoordsToPixel({x, y})));
}

/**
 * @brief Draws one region of a sprite sheet inside a destination rectangle.
 *
 * Keep transparent margins so the original FreeGUI shadows remain visible.
 *
 * @param window Render target receiving the sprite.
 * @param texture Sprite sheet containing the requested element.
 * @param source Pixel rectangle inside the sprite sheet.
 * @param destination Rectangle in the current logical view.
 */
inline void draw_texture_region(sf::RenderWindow& window, const sf::Texture& texture,
                                const sf::IntRect& source,
                                const sf::FloatRect& destination) {
    sf::Sprite sprite(texture, source);
    sprite.setPosition(destination.left, destination.top);
    sprite.setScale(destination.width / static_cast<float>(source.width),
                    destination.height / static_cast<float>(source.height));
    window.draw(sprite);
}

// Sprite rectangles refer to the original FreeGUI sheets, without modifying them.
class GuiSkin {
public:
    sf::Texture buttons;
    sf::Texture windows;

    void surface(sf::RenderWindow& window, const sf::FloatRect& bounds,
                 bool inset = false) const {
        const sf::IntRect source = inset ? sf::IntRect(3055, 950, 825, 475)
                                        : sf::IntRect(2040, 250, 930, 660);
        constexpr int edge = 70;
        const float corner = inset ? 8.f : 15.f;
        const int sx[] = {source.left, source.left + edge, source.left + source.width - edge};
        const int sy[] = {source.top, source.top + edge, source.top + source.height - edge};
        const int sw[] = {edge, source.width - 2 * edge, edge};
        const int sh[] = {edge, source.height - 2 * edge, edge};
        const float dx[] = {bounds.left, bounds.left + corner, bounds.left + bounds.width - corner};
        const float dy[] = {bounds.top, bounds.top + corner, bounds.top + bounds.height - corner};
        const float dw[] = {corner, bounds.width - 2.f * corner, corner};
        const float dh[] = {corner, bounds.height - 2.f * corner, corner};
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                draw_texture_region(window, windows, {sx[x], sy[y], sw[x], sh[y]},
                                    {dx[x], dy[y], dw[x], dh[y]});
            }
        }
    }

    void centered(sf::RenderWindow& window, const sf::Font& font, const std::string& text,
                  const sf::FloatRect& bounds, unsigned size, sf::Color color = style::ink,
                  bool outlined = false) const {
        auto label = make_ui_text(window, font, text, size);
        label.setStyle(sf::Text::Bold);
        label.setFillColor(color);
        if (outlined) {
            label.setOutlineColor(sf::Color(117, 40, 42));
            label.setOutlineThickness(1.f / label.getScale().x);
        }
        const auto local = label.getLocalBounds();
        const float scale = label.getScale().x;
        position_ui_text(window, label,
            bounds.left + (bounds.width - local.width * scale) / 2.f - local.left * scale,
            bounds.top + (bounds.height - local.height * scale) / 2.f - local.top * scale);
        window.draw(label);
    }

    void panel(sf::RenderWindow& window, const sf::Font& font, const std::string& title,
               const sf::FloatRect& bounds) const {
        surface(window, bounds);
        const sf::FloatRect ribbon{bounds.left + (bounds.width - 230.f) / 2.f,
                                   bounds.top - 21.f, 230.f, 61.f};
        draw_texture_region(window, windows, {2140, 25, 720, 205}, ribbon);
        centered(window, font, title, {ribbon.left, ribbon.top + 2.f, ribbon.width, 39.f},
                 22, sf::Color::White, true);
    }

    void button(sf::RenderWindow& window, const sf::Font& font, const sf::FloatRect& bounds,
                const std::string& label, sf::Vector2f mouse, bool pressed,
                bool enabled = true, int color = 0, unsigned size = 19) const {
        const int source_x[] = {175, 632, 1085, 1540}; // blue, green, red, grey
        const int index = enabled ? color : 3;
        sf::Sprite sprite(buttons, {source_x[index], 430, 430, 195});
        const bool hover = enabled && bounds.contains(mouse);
        const float offset = hover && pressed ? 2.f : 0.f;
        sprite.setPosition(bounds.left, bounds.top + offset);
        sprite.setScale(bounds.width / 430.f, bounds.height / 195.f);
        if (hover) {
            sprite.setColor(sf::Color(255, 240, 225));
        }
        window.draw(sprite);
        centered(window, font, label,
                 {bounds.left, bounds.top - 3.f + offset, bounds.width, bounds.height},
                 size, sf::Color::White, true);
    }

    void icon(sf::RenderWindow& window, const sf::IntRect& source,
              const sf::FloatRect& bounds) const {
        draw_texture_region(window, buttons, source, bounds);
    }
};

} // namespace ntype::client
