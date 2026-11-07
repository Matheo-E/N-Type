#pragma once

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include "GuiSkin.hpp"
#include "Protocol.hpp"

namespace ntype::client {

// The results screen's "Continuer" button, in game coordinates (1280x720).
inline const sf::FloatRect kResultsContinue{490.f, 572.f, 300.f, 72.f};

/**
 * @brief Game over or victory, over the game: the level reached, every member's score
 * (best first, in their colour), the total of everyone's points, and "Continuer".
 * @param window Window whose current view is the 1280x720 game view.
 * @param gui FreeGUI skin of the pause menu.
 * @param lobby The lobby once its game ended: result, level reached and scores.
 */
void draw_results_screen(sf::RenderWindow& window, const GuiSkin& gui, const sf::Font& font,
                         const protocol::LobbyState& lobby, sf::Vector2f mouse, bool pressed);

} // namespace ntype::client
