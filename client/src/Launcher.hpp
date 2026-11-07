#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include "ClientNetwork.hpp"
#include "WindowState.hpp"

namespace ntype::client {

struct ConnectionSettings {
    std::string name;
    std::string host;
    std::uint16_t port;
    bool music_enabled = true;
    bool click_sound_enabled = true;
};

/** @brief Where the launcher opens. */
enum class LauncherStart {
    Title,       // First launch: title screen, then the connection form.
    Lobbies,     // The player left a game: the lobby list.
    WaitingRoom, // The game ended and the lobby is waiting again.
};

/**
 * @brief Runs the menus until the player's lobby starts playing, or the window closes.
 *
 * The session is created by the connection page and kept across the menus and
 * the game, because lobby membership belongs to that connection.
 *
 * @param window Shared application window reused by the lobby and game.
 * @param window_state Window preferences retained across all screens.
 * @param settings Connection values and sound preferences, updated by the menus.
 * @param session The server session; created, replaced or dropped by the menus.
 * @param start Page shown first.
 * @return true when the lobby is playing and the game should be shown.
 */
bool show_launcher(sf::RenderWindow& window, WindowState& window_state, ConnectionSettings& settings,
                   std::unique_ptr<ClientNetwork>& session, LauncherStart start = LauncherStart::Title);

} // namespace ntype::client
