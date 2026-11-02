#include "GameScreen.hpp"

#include <algorithm>

#include "Factory.hpp"
#include "GameConfig.hpp"
#include "System.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

GameScreen::GameScreen(const ui::Theme& theme, ClientNetwork& network) : theme_(theme), network_(network) {}

void GameScreen::sync_world() {
    const auto& players = network_.players();
    // Only players in the latest snapshot keep an entity.
    for (auto it = entities_.begin(); it != entities_.end();) {
        const auto id = it->first;
        if (std::none_of(players.begin(), players.end(), [id](const auto& player) { return player.id == id; })) {
            world_.Positions.erase(it->second);
            world_.Rectangles.erase(it->second);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& player : players) {
        auto [it, added] = entities_.try_emplace(player.id, 0);
        if (added) {
            it->second = Create_E(world_);
            auto& square = world_.Rectangles[it->second].Rect;
            square.setSize({config::kPlayerSize, config::kPlayerSize});
            square.setFillColor(theme_.players[player.id - 1U]);
        }
        // The server is authoritative: positions are copied, never simulated here.
        world_.Positions[it->second].Xy = {static_cast<float>(player.x), static_cast<float>(player.y)};
    }
}

void GameScreen::draw(sf::RenderWindow& window) {
    sync_world();
    Draw(world_, window);
    ui::draw_label(window, theme_, "Esc: leave the lobby", {ui::kMenuWidth - 200, 12}, theme_.small_size,
                   theme_.muted);
}

} // namespace ntype::client
