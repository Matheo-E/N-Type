#pragma once

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

// The lobby before (and between) games: room code and members. The leader
// also renames the lobby and starts the game.
class WaitingRoomScreen : public Screen {
public:
    WaitingRoomScreen(const ui::Theme& theme, ClientNetwork& network);
    void handle(const sf::Event& event, sf::Vector2f point) override;
    void update() override;
    void draw(sf::RenderWindow& window) override;

private:
    void rename();

    const ui::Theme& theme_;
    ClientNetwork& network_;
    bool leader_ = false;
    ui::TextField name_;
    ui::Button rename_;
    ui::Button start_;
    ui::Button leave_;
};

} // namespace ntype::client
