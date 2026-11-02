#include "Widgets.hpp"

#include <algorithm>
#include <cctype>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include "Protocol.hpp"

namespace ntype::client::ui {

namespace {

bool left_click(const sf::Event& event) {
    return event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left;
}

sf::RectangleShape box(const sf::FloatRect& bounds, const sf::Color fill) {
    sf::RectangleShape shape({bounds.width, bounds.height});
    shape.setPosition(bounds.left, bounds.top);
    shape.setFillColor(fill);
    return shape;
}

sf::Color style_color(const Theme& theme, const Style style) {
    switch (style) {
    case Style::Secondary:
        return theme.secondary;
    case Style::Danger:
        return theme.danger;
    case Style::Primary:
        break;
    }
    return theme.primary;
}

} // namespace

void draw_label(sf::RenderTarget& target, const Theme& theme, const std::string& text, const sf::Vector2f position,
                const unsigned size, const sf::Color color, const bool centered) {
    sf::Text label(sf::String::fromUtf8(text.begin(), text.end()), theme.font, size);
    label.setFillColor(color);
    if (centered) {
        const sf::FloatRect local = label.getLocalBounds();
        label.setOrigin(local.left + local.width / 2.0F, local.top + local.height / 2.0F);
    }
    label.setPosition(position);
    target.draw(label);
}

void draw_panel(sf::RenderTarget& target, const Theme& theme, const sf::FloatRect bounds) {
    sf::RectangleShape shape = box(bounds, theme.panel);
    if (theme.panel_texture) {
        shape.setTexture(theme.panel_texture);
        shape.setFillColor(sf::Color::White);
    }
    target.draw(shape);
}

bool Button::handle(const sf::Event& event, const sf::Vector2f point) const {
    return enabled && left_click(event) && bounds.contains(point);
}

void Button::draw(sf::RenderTarget& target, const Theme& theme) const {
    sf::RectangleShape shape = box(bounds, enabled ? style_color(theme, style) : theme.disabled);
    if (theme.button_texture) {
        shape.setTexture(theme.button_texture);
        shape.setFillColor(enabled ? sf::Color::White : theme.disabled);
    }
    target.draw(shape);
    draw_label(target, theme, label, {bounds.left + bounds.width / 2.0F, bounds.top + bounds.height / 2.0F},
               theme.text_size, enabled ? theme.text : theme.muted, true);
}

bool TextField::handle(const sf::Event& event, const sf::Vector2f point) {
    if (left_click(event)) {
        focused = bounds.contains(point);
        return false;
    }
    if (!focused || event.type != sf::Event::TextEntered) {
        return false;
    }
    const sf::Uint32 unicode = event.text.unicode;
    if (unicode == 13) {
        return true;
    }
    if (unicode == 8) {
        if (!value.empty()) {
            value.pop_back();
        }
        return false;
    }
    if (unicode < 32 || unicode > 126 || value.size() >= max_length) {
        return false;
    }
    auto c = static_cast<char>(unicode);
    if (filter == Filter::Code) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (protocol::kCodeAlphabet.find(c) == std::string_view::npos) {
            return false;
        }
    }
    value.push_back(c);
    return false;
}

void TextField::draw(sf::RenderTarget& target, const Theme& theme) const {
    sf::RectangleShape shape = box(bounds, theme.panel);
    shape.setOutlineThickness(2.0F);
    shape.setOutlineColor(focused ? theme.focus : theme.border);
    target.draw(shape);
    const std::string shown = focused ? value + "_" : value;
    draw_label(target, theme, shown, {bounds.left + theme.padding, bounds.top + (bounds.height - static_cast<float>(theme.text_size)) / 2.0F - 3.0F},
               theme.text_size, theme.text);
}

std::size_t ListView::visible_rows() const {
    return static_cast<std::size_t>(bounds.height / row_height);
}

ListEvent ListView::handle(const sf::Event& event, const sf::Vector2f point) {
    if (event.type == sf::Event::MouseWheelScrolled) {
        const std::size_t max_scroll =
            rows.size() > visible_rows() ? rows.size() - visible_rows() : 0;
        if (event.mouseWheelScroll.delta < 0 && scroll < max_scroll) {
            ++scroll;
        } else if (event.mouseWheelScroll.delta > 0 && scroll > 0) {
            --scroll;
        }
        return ListEvent::None;
    }
    if (!left_click(event) || !bounds.contains(point)) {
        return ListEvent::None;
    }
    const std::size_t row = scroll + static_cast<std::size_t>((point.y - bounds.top) / row_height);
    if (row >= rows.size()) {
        return ListEvent::None;
    }
    const bool again = clicked == row && since_click.getElapsedTime() < sf::milliseconds(400);
    selected = row;
    if (again) {
        clicked.reset(); // A third click starts over.
        return ListEvent::Activated;
    }
    clicked = row;
    since_click.restart();
    return ListEvent::Selected;
}

void ListView::draw(sf::RenderTarget& target, const Theme& theme) const {
    const std::size_t last = std::min(rows.size(), scroll + visible_rows());
    for (std::size_t row = scroll; row < last; ++row) {
        const sf::FloatRect line{bounds.left, bounds.top + static_cast<float>(row - scroll) * row_height,
                                 bounds.width, row_height - 4.0F};
        sf::RectangleShape shape = box(line, theme.panel);
        if (selected == row) {
            shape.setOutlineThickness(2.0F);
            shape.setOutlineColor(theme.focus);
        }
        target.draw(shape);
        for (std::size_t column = 0; column < rows[row].size(); ++column) {
            const float x = column < columns.size() ? columns[column] : 0.0F;
            draw_label(target, theme, rows[row][column], {line.left + theme.padding + x, line.top + 10.0F},
                       theme.text_size, column == 0 ? theme.text : theme.muted);
        }
    }
}

} // namespace ntype::client::ui
