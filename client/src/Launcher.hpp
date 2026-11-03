#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

namespace ntype::client {

struct ConnectionSettings {
    std::string name;
    std::string host;
    std::uint16_t port;
    bool music_enabled = true;
};

/**
 * @brief Displays the connection screen or returns directly to the lobby browser.
 * @param window Shared application window reused by the lobby and game.
 * @param fullscreen Shared fullscreen state.
 * @param defaults Values retained from the previous session.
 * @param open_browser If true, skips the connection form when returning from a game.
 */
std::optional<ConnectionSettings> show_launcher(sf::RenderWindow& window, bool& fullscreen,
                                                const ConnectionSettings& defaults,
                                                bool open_browser = false);

} // namespace ntype::client
