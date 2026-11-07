#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Audio/Music.hpp>
#include <SFML/System/Clock.hpp>

#include "ClientNetwork.hpp"
#include "World.hpp"
#include "GuiSkin.hpp"
#include "WindowState.hpp"
#include "ControlsMenu.hpp"

namespace ntype::client {

/** @brief Why the game screen closed. */
enum class GameExitAction {
    Quit,        // The window was closed.
    Lobby,       // The player left the lobby (or lost it): back to the lobby list.
    WaitingRoom, // The lobby went back to waiting (game over): back to its waiting room.
};

/** @brief Graphical client that mirrors server snapshots and renders the selected level. */
class ClientApp {
public:
    /**
     * @brief Shows the game of the session's lobby and loads the first adventure background.
     * @param window Shared application window.
     * @param window_state Window preferences retained across all screens.
     * @param network Session already inside a playing lobby; it outlives this screen.
     * @param music_enabled Initial music preference retained across screens.
     */
    ClientApp(sf::RenderWindow& window, WindowState& window_state, ClientNetwork& network,
              bool music_enabled);
    /** @brief Runs the game until the lobby stops playing, the player leaves, or the window closes. */
    GameExitAction run();
    bool music_enabled() const { return music_enabled_; }

private:
    void poll_events();
    std::uint8_t read_input() const;
    void sync_world();
    void render();
    void create_window();
    void update_view();
    /** @brief Draws four horizontally repeated layers at configured speeds. */
    void draw_parallax();
    void draw_parallax_layers(sf::RenderTarget& target,
                              const std::array<sf::Texture, 4>& textures,
                              sf::Uint8 opacity = 255);
    void draw_pause_menu();
    void update_adventure(float elapsed_seconds);
    void load_background(std::size_t level, std::size_t ambience);
    void load_background_layers(std::array<sf::Texture, 4>& textures,
                                std::size_t level, std::size_t ambience);
    void load_level_music(std::size_t level);

    ClientNetwork& network_;
    sf::RenderWindow& window_;
    WindowState& window_state_;
    sf::Font ui_font_;
    GuiSkin pause_gui_;
    ControlsMenu controls_menu_;
    sf::Music music_;
    std::array<sf::Texture, 4> background_textures_;
    std::array<sf::Texture, 4> transition_background_textures_;
    sf::Clock frame_clock_;
    double automatic_background_x_ = 0.0;
    bool pause_menu_open_ = false;
    bool music_enabled_ = true;
    bool session_active_ = true;
    float adventure_elapsed_seconds_ = 0.f;
    std::size_t current_level_ = 0;
    std::size_t current_ambience_ = 0;
    std::size_t transition_ambience_ = 1;
    float transition_elapsed_seconds_ = 0.f;
    bool ambience_transition_active_ = false;
    GameExitAction exit_action_ = GameExitAction::Quit;
    // Engine world mirroring the latest snapshot: one entity per player.
    World world_;
    std::map<std::uint16_t, int> entities_; // player id -> engine entity
};

} // namespace ntype::client
