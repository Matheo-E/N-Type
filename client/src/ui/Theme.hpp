#pragma once

#include <array>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "GameConfig.hpp"

namespace ntype::client::ui {

// Menus are laid out in this view, letterboxed into the window.
inline constexpr float kMenuWidth = 1280.0F;
inline constexpr float kMenuHeight = 720.0F;
inline constexpr std::array<const char*, config::kMapCount> kMapNames{"Blue", "Green", "Red"};

// The only place that knows what the menus look like. When menu art arrives,
// set the textures here: widgets use them instead of plain rectangles.
struct Theme {
    sf::Font font;
    sf::Color background{20, 24, 34};
    sf::Color panel{36, 43, 58};
    sf::Color border{90, 98, 115};
    sf::Color focus{80, 180, 255};
    sf::Color text{sf::Color::White};
    sf::Color muted{155, 165, 185};
    sf::Color primary{48, 145, 95};
    sf::Color secondary{55, 115, 180};
    sf::Color danger{175, 70, 70};
    sf::Color disabled{75, 80, 90};
    sf::Color error{255, 120, 120};
    std::array<sf::Color, config::kMapCount> maps{{{55, 115, 180}, {55, 145, 85}, {175, 80, 70}}};
    std::array<sf::Color, config::kMaxPlayers> players{
        {{80, 180, 255}, {255, 160, 60}, {110, 220, 110}, {240, 100, 200}}};
    unsigned title_size = 40;
    unsigned text_size = 22;
    unsigned small_size = 16;
    float padding = 12.0F;
    const sf::Texture* button_texture = nullptr;
    const sf::Texture* panel_texture = nullptr;
};

// Uses a system font until the project ships its own; throws if none is found.
Theme load_theme();

} // namespace ntype::client::ui
