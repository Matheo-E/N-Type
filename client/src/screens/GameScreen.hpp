#pragma once

#include <map>

#include "ClientNetwork.hpp"
#include "Screen.hpp"
#include "World.hpp"
#include "ui/Theme.hpp"

namespace ntype::client {

// Draws the lobby's game from the server's snapshots.
class GameScreen : public Screen {
public:
    GameScreen(const ui::Theme& theme, ClientNetwork& network);
    void handle(const sf::Event&, sf::Vector2f) override {}
    void draw(sf::RenderWindow& window) override;

private:
    void sync_world();

    const ui::Theme& theme_;
    ClientNetwork& network_;
    // Engine world mirroring the latest snapshot: one entity per player.
    World world_;
    std::map<std::uint16_t, int> entities_; // slot -> engine entity
};

} // namespace ntype::client
