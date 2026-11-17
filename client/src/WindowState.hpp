#pragma once

#include <string>
#include <SFML/Graphics/RenderWindow.hpp>

namespace ntype::client {

/** Window preferences shared by the launcher and every game session. */
struct WindowState {
    bool fullscreen = false;
    sf::Vector2u windowed_size{1000, 700};
    sf::Vector2i windowed_position{};
    bool has_windowed_position = false;

    void open(sf::RenderWindow& window, const std::string& title) const {
        // A screen transition reuses the native window and its current bounds.
        if (!window.isOpen()) {
            window.create(fullscreen ? sf::VideoMode::getDesktopMode()
                                     : sf::VideoMode(windowed_size.x, windowed_size.y),
                          title, fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
            if (!fullscreen && has_windowed_position) window.setPosition(windowed_position);
        } else {
            window.setTitle(title);
        }
        window.setFramerateLimit(60);
    }

    void toggle_fullscreen(sf::RenderWindow& window, const std::string& title) {
        if (!fullscreen) {
            windowed_size = window.getSize();
            windowed_position = window.getPosition();
            has_windowed_position = true;
        }
        fullscreen = !fullscreen;
        window.create(fullscreen ? sf::VideoMode::getDesktopMode()
                                 : sf::VideoMode(windowed_size.x, windowed_size.y),
                      title, fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
        if (!fullscreen && has_windowed_position) window.setPosition(windowed_position);
        window.setFramerateLimit(60);
    }
};

} // namespace ntype::client
