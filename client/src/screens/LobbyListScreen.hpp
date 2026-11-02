#pragma once

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

// Current lobbies (waiting or playing), join one or type a room code, or create one.
class LobbyListScreen : public Screen {
public:
    LobbyListScreen(const ui::Theme& theme, AppActions& actions, ClientNetwork& network);
    void handle(const sf::Event& event, sf::Vector2f point) override;
    void update() override;
    void draw(sf::RenderWindow& window) override;

private:
    void join_selected();

    const ui::Theme& theme_;
    AppActions& actions_;
    ClientNetwork& network_;
    ui::ListView list_;
    std::vector<std::string> codes_; // code of each list row
    std::string selected_code_;      // keeps the selection when the list refreshes
    ui::Button join_selected_;
    ui::TextField code_;
    ui::Button join_code_;
    ui::Button create_;
};

} // namespace ntype::client
