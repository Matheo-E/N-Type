#pragma once

#include <array>
#include <cstdint>

#include <SFML/Graphics/Color.hpp>

#include "GameConfig.hpp"

namespace ntype::client {

// One colour per lobby slot (1 to 4): the waiting room's swatches and the halo
// around each player's tooth in game.
inline const std::array<sf::Color, config::kMaxPlayers> kPlayerColors{
    sf::Color(66, 135, 245), sf::Color(245, 150, 40), sf::Color(80, 190, 90), sf::Color(240, 110, 180)};
inline constexpr std::array<const char*, config::kMaxPlayers> kPlayerColorNames{"blue", "orange", "green",
                                                                                 "pink"};

/** @brief Colour of a slot; out-of-range slots wrap instead of failing. */
inline sf::Color player_color(const std::uint16_t slot) {
    return kPlayerColors[(slot + kPlayerColors.size() - 1U) % kPlayerColors.size()];
}

} // namespace ntype::client
