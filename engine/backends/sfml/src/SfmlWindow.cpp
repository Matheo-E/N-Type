#include <SFML/Window/Event.hpp>

#include <ntype/engine/sfml/KeyMapping.hpp>
#include <ntype/engine/sfml/SfmlWindow.hpp>

namespace ntype::engine::sfml {

SfmlWindow::SfmlWindow(const math::Vec2u size, const std::string& title)
    : window_(sf::VideoMode({size.x, size.y}), title) {
    window_.setVerticalSyncEnabled(true);
    window_.setFramerateLimit(144);
}

std::vector<platform::InputEvent> SfmlWindow::poll_events() {
    std::vector<platform::InputEvent> events;
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            events.emplace_back(platform::WindowClosed{});
        } else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
            events.emplace_back(platform::WindowResized{{resized->size.x, resized->size.y}});
        } else if (event->is<sf::Event::FocusLost>()) {
            events.emplace_back(platform::WindowFocusChanged{false});
        } else if (event->is<sf::Event::FocusGained>()) {
            events.emplace_back(platform::WindowFocusChanged{true});
        } else if (const auto* pressed = event->getIf<sf::Event::KeyPressed>()) {
            events.emplace_back(platform::KeyPressed{from_sfml(pressed->code)});
        } else if (const auto* released = event->getIf<sf::Event::KeyReleased>()) {
            events.emplace_back(platform::KeyReleased{from_sfml(released->code)});
        } else if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
            events.emplace_back(platform::TextEntered{text->unicode});
        }
    }
    return events;
}

bool SfmlWindow::is_open() const {
    return window_.isOpen();
}

void SfmlWindow::close() {
    window_.close();
}

math::Vec2u SfmlWindow::size() const {
    const sf::Vector2u size = window_.getSize();
    return {size.x, size.y};
}

bool SfmlWindow::has_focus() const {
    return window_.hasFocus();
}

} // namespace ntype::engine::sfml
