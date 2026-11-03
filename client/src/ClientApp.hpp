#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Audio/Music.hpp>
#include <SFML/System/Clock.hpp>

#include "ClientNetwork.hpp"
#include "World.hpp"

namespace ntype::client {

enum class GameExitAction { Quit, Restart, Lobby };

/** @brief Graphical client that mirrors server snapshots and renders the selected level. */
class ClientApp {
public:
    /**
     * @brief Creates the network client and loads one of the six parallax themes.
     * @param window Shared application window.
     * @param fullscreen Shared fullscreen state.
     * @param name Player display name.
     * @param host Server hostname or address.
     * @param port UDP server port.
     * @param music_enabled Initial music preference retained across screens.
     */
    ClientApp(sf::RenderWindow& window, bool& fullscreen, const std::string& name,
              const std::string& host, std::uint16_t port, bool music_enabled);
    /** @brief Runs one game session until quit, restart, or return-to-lobby is selected. */
    GameExitAction run();
    bool music_enabled() const { return music_enabled_; }

private:
    std::string name_;
    void poll_events();
    std::uint8_t read_input() const;
    void sync_world();
    void render();
    void create_window(bool recreate = false);
    void update_view();
    /** @brief Smoothly follows the local player's horizontal server position. */
    void update_camera(float elapsed_seconds);
    /** @brief Draws four horizontally repeated layers at configured speeds. */
    void draw_parallax();
    void draw_pause_menu();
    void update_adventure(float elapsed_seconds);
    void load_background(std::size_t level, std::size_t ambience);
    void load_level_music(std::size_t level);

    ClientNetwork network_;
    sf::RenderWindow& window_;
    bool& fullscreen_;
    sf::Texture player_texture_;
    sf::Texture menu_texture_;
    sf::Music music_;
    std::array<sf::Texture, 4> background_textures_;
    sf::Clock animation_clock_;
    sf::Clock frame_clock_;
    float camera_x_ = 0.f;
    bool camera_initialized_ = false;
    bool pause_menu_open_ = false;
    bool music_enabled_ = true;
    bool session_active_ = true;
    float adventure_elapsed_seconds_ = 0.f;
    std::size_t current_level_ = 0;
    std::size_t current_ambience_ = 0;
    GameExitAction exit_action_ = GameExitAction::Quit;
    // Engine world mirroring the latest snapshot: one entity per player.
    World world_;
    std::map<std::uint16_t, int> entities_; // player id -> engine entity
    std::map<std::uint16_t, sf::Vector2f> previous_positions_;
    std::map<std::uint16_t, float> last_movement_times_;
};

} // namespace ntype::client
