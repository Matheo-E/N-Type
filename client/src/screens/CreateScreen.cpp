#include "CreateScreen.hpp"

namespace ntype::client {

CreateScreen::CreateScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network)
    : theme_(theme), actions_(actions), network_(network),
      name_{{400, 260, 480, 56}, network.name() + "'s lobby", protocol::kMaxLobbyName},
      create_{"Create", {400, 360, 230, 56}}, back_{"Back", {650, 360, 230, 56}, ui::Style::Secondary} {
    name_.focused = true;
}

void CreateScreen::handle(const sf::Event& event, const sf::Vector2f point) {
    const bool submitted = name_.handle(event, point);
    if ((submitted || create_.handle(event, point)) && create_.enabled) {
        network_.create(name_.value);
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
    ui::draw_label(window, theme_, "Create a lobby", {ui::kMenuWidth / 2.0F, 120}, theme_.title_size, theme_.text,
                   true);
    ui::draw_label(window, theme_, "Lobby name", {400, 225}, theme_.text_size, theme_.text);
    name_.draw(window, theme_);
    create_.draw(window, theme_);
    back_.draw(window, theme_);
    if (!network_.error().empty()) {
        ui::draw_label(window, theme_, network_.error(), {ui::kMenuWidth / 2.0F, 450}, theme_.small_size,
                       theme_.error, true);
    }
    ui::draw_label(window, theme_, "Enter: create   Esc: back", {ui::kMenuWidth / 2.0F, 680}, theme_.small_size,
                   theme_.muted, true);
}

} // namespace ntype::client
