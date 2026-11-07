#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>

#include "GameConfig.hpp"
#include "Protocol.hpp"

namespace ntype::client {

/**
 * @brief In-game scoreboard: one FreeGUI card per lobby member, in the slot's colour.
 *
 * Each card is a glossy tile in the player's colour holding a tooth portrait,
 * attached to a cream plate with the name and a star score. A spectator's tile
 * is grey and "spectateur" replaces the score.
 * Letterboxing leaves black bars around the 1280x720 play area when the window
 * is not 16:9: the cards go in the top bar and the lobby name and code in the
 * bottom one. Without room (a 16:9 window) the cards sit over the top edge of
 * the play area, left of the menu icon.
 *
 * Drawing happens in target pixels, outside the game view; the caller restores
 * its own view afterwards.
 */
class Hud {
public:
    // The boss of the level, while it is on screen.
    struct Boss {
        std::string name;
        std::uint8_t hp = 0; // percent
    };

    /**
     * @param font Text font.
     * @param windows FreeGUI Window.png (plates and star).
     * @param tooth Player sprite sheet; its first frame is the portrait.
     * @param asset_dir Assets folder: the glossy tile is recoloured from FreeGUI/png/Button.png.
     * @throws std::runtime_error if Button.png cannot be read.
     */
    Hud(const sf::Font& font, const sf::Texture& windows, const sf::Texture& tooth,
        const std::string& asset_dir);

    /**
     * @param target Window (or texture) to draw on.
     * @param lobby Members, scores and the receiving player's slot.
     * @param entities Latest frame: each player's health, shown on its card.
     * @param boss The boss's name and health, shown at the bottom of the play area.
     * @param play Play area in target pixels, as letterboxed by the game view.
     * @param menu_left Target x where the menu icon starts (kept free without bars).
     */
    void draw(sf::RenderTarget& target, const protocol::LobbyState& lobby,
              const std::vector<protocol::Entity>& entities, const std::optional<Boss>& boss,
              const sf::FloatRect& play, float menu_left) const;

private:
    void draw_card(sf::RenderTarget& target, const protocol::LobbyMember& member, bool you,
                   std::optional<std::uint8_t> hp, const sf::FloatRect& bounds) const;
    void draw_boss(sf::RenderTarget& target, const Boss& boss, const sf::FloatRect& play) const;
    void draw_plate(sf::RenderTarget& target, const sf::IntRect& source, int cap,
                    const sf::FloatRect& bounds) const;

    const sf::Font& font_;
    const sf::Texture& windows_;
    const sf::Texture& tooth_;
    std::array<sf::Texture, config::kMaxPlayers> tiles_; // the glossy tile in each slot's colour
    sf::Texture spectator_tile_;                          // grey: watching until the next level
};

} // namespace ntype::client
