#pragma once

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

// Lobby name, then Create: the creator becomes the leader.
class CreateScreen : public Screen {
public:
    CreateScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network);
    void handle(const sf::Event& event, sf::Vector2f point) override;
    void update() override;
    void draw(sf::RenderWindow& window) override;

private:
    const ui::Theme& theme_;
    AppActions& actions_;
    ClientNetwork& network_;
    ui::TextField name_;
    ui::Button create_;
    ui::Button back_;
};

} // namespace ntype::client
