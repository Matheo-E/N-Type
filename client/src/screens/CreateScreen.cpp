#include "CreateScreen.hpp"

#include <SFML/Graphics/RectangleShape.hpp>

namespace ntype::client {

void draw_map_cards(sf::RenderTarget& target, const ui::Theme& theme,
                    const std::array<sf::FloatRect, config::kMapCount>& cards, const std::uint8_t selected) {
    for (std::uint8_t i = 0; i < cards.size(); ++i) {
        sf::RectangleShape card({cards[i].width, cards[i].height});
        card.setPosition(cards[i].left, cards[i].top);
        card.setFillColor(theme.maps[i]);
        card.setOutlineThickness(i == selected ? 4.0F : 0.0F);
        card.setOutlineColor(theme.text);
        target.draw(card);
        ui::draw_label(target, theme, ui::kMapNames[i],
                       {cards[i].left + cards[i].width / 2.0F, cards[i].top + cards[i].height / 2.0F},
                       theme.text_size, theme.text, true);
    }
}

std::optional<std::uint8_t> map_card_at(const std::array<sf::FloatRect, config::kMapCount>& cards,
                                        const sf::Event& event, const sf::Vector2f point) {
    if (event.type != sf::Event::MouseButtonPressed || event.mouseButton.button != sf::Mouse::Left) {
        return std::nullopt;
    }
    for (std::uint8_t i = 0; i < cards.size(); ++i) {
        if (cards[i].contains(point)) {
            return i;
        }
    }
    return std::nullopt;
}

CreateScreen::CreateScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network)
    : theme_(theme), actions_(actions), network_(network),
      name_{{400, 200, 480, 56}, network.name() + "'s lobby", protocol::kMaxLobbyName},
      maps_{{{400, 330, 150, 110}, {565, 330, 150, 110}, {730, 330, 150, 110}}},
      create_{"Create", {400, 490, 230, 56}}, back_{"Back", {650, 490, 230, 56}, ui::Style::Secondary} {
    name_.focused = true;
}

void CreateScreen::handle(const sf::Event& event, const sf::Vector2f point) {
    if (const auto map = map_card_at(maps_, event, point)) {
        map_ = *map;
    }
    const bool submitted = name_.handle(event, point);
    if ((submitted || create_.handle(event, point)) && create_.enabled) {
        network_.create(name_.value, map_);
    }
    if (back_.handle(event, point)) {
        network_.clear_error();
        actions_.show_create(false);
    }
}

void CreateScreen::update() {
    create_.enabled = !name_.value.empty() && !network_.busy();
}

void CreateScreen::draw(sf::RenderWindow& window) {
    ui::draw_label(window, theme_, "Create a lobby", {ui::kMenuWidth / 2.0F, 90}, theme_.title_size, theme_.text,
                   true);
    ui::draw_label(window, theme_, "Lobby name", {400, 165}, theme_.text_size, theme_.text);
    name_.draw(window, theme_);
    ui::draw_label(window, theme_, "Map", {400, 295}, theme_.text_size, theme_.text);
    draw_map_cards(window, theme_, maps_, map_);
    create_.draw(window, theme_);
    back_.draw(window, theme_);
    if (!network_.error().empty()) {
        ui::draw_label(window, theme_, network_.error(), {ui::kMenuWidth / 2.0F, 580}, theme_.small_size,
                       theme_.error, true);
    }
    ui::draw_label(window, theme_, "Enter: create   Esc: back", {ui::kMenuWidth / 2.0F, 680}, theme_.small_size,
                   theme_.muted, true);
}

} // namespace ntype::client
