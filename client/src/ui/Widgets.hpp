#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>

#include "Theme.hpp"

// Small menu widgets. They hold no game logic: screens read their state and
// call the network. `point` is the mouse position mapped to the menu view.
namespace ntype::client::ui {

enum class Style { Primary, Secondary, Danger };

struct Button {
    std::string label{};
    sf::FloatRect bounds{};
    Style style = Style::Primary;
    bool enabled = true;

    // True on a left click inside, while enabled.
    bool handle(const sf::Event& event, sf::Vector2f point) const;
    void draw(sf::RenderTarget& target, const Theme& theme) const;
};

enum class Filter {
    Printable, // ASCII 32 to 126
    Code,      // Room code characters; lower case is upper-cased
};

struct TextField {
    sf::FloatRect bounds{};
    std::string value{};
    std::size_t max_length = 16;
    Filter filter = Filter::Printable;
    bool focused = false;

    // A click focuses or unfocuses it; typing edits it. True on Enter while focused.
    bool handle(const sf::Event& event, sf::Vector2f point);
    void draw(sf::RenderTarget& target, const Theme& theme) const;
};

enum class ListEvent { None, Selected, Activated };

struct ListView {
    sf::FloatRect bounds{};
    float row_height = 48.0F;
    std::vector<std::vector<std::string>> rows{};
    std::vector<float> columns{}; // x offset of each column inside the row
    std::optional<std::size_t> selected{};
    std::size_t scroll = 0;     // first visible row

    // A click selects a row; a second click on it within 400 ms activates it.
    ListEvent handle(const sf::Event& event, sf::Vector2f point);
    void draw(sf::RenderTarget& target, const Theme& theme) const;
    std::size_t visible_rows() const;

    // Double-click detection (internal).
    sf::Clock since_click{};
    std::optional<std::size_t> clicked{};
};

void draw_label(sf::RenderTarget& target, const Theme& theme, const std::string& text, sf::Vector2f position,
                unsigned size, sf::Color color, bool centered = false);
void draw_panel(sf::RenderTarget& target, const Theme& theme, sf::FloatRect bounds);

} // namespace ntype::client::ui
