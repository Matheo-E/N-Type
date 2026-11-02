#include "GameScreen.hpp"

#include <algorithm>

#include "Factory.hpp"
#include "GameConfig.hpp"
#include "System.hpp"
#include "ui/Widgets.hpp"

namespace ntype::client {

GameScreen::GameScreen(const ui::Theme& theme, ClientNetwork& network) : theme_(theme), network_(network) {}

void GameScreen::sync_world() {
    const std::vector<protocol::Entity>& players = network_.players();
    // Only players in the latest snapshot keep an entity.
    for (auto it = entities_.begin(); it != entities_.end();) {
        const std::uint16_t id = it->first;
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
            sf::RectangleShape& square = world_.Rectangles[it->second].Rect;
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
    if (network_.is_spectator()) {
        const sf::FloatRect banner{ui::kMenuWidth / 2.0F - 330, 20, 660, 52};
        ui::draw_panel(window, theme_, banner);
        ui::draw_label(window, theme_, "Spectating \xE2\x80\x94 you'll join at the next level",
                       {banner.left + banner.width / 2.0F, banner.top + banner.height / 2.0F}, theme_.text_size,
                       theme_.text, true);
    }
    ui::draw_label(window, theme_, "Esc: leave the lobby", {ui::kMenuWidth - 200, 12}, theme_.small_size,
                   theme_.muted);
}

} // namespace ntype::client
