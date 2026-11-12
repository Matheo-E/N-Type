#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Audio/Music.hpp>
#include <SFML/System/Clock.hpp>

#include "ClientNetwork.hpp"
#include "World.hpp"
#include "BeamPatterns.hpp"
#include "ZonePatterns.hpp"
#include "GuiSkin.hpp"
#include "WindowState.hpp"
#include "ControlsMenu.hpp"
#include "ExplosionEffects.hpp"
#include "ImpactSounds.hpp"
#include "Hud.hpp"

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
    /** @brief Game over or victory: every member's score, the total, then the waiting room. */
    void draw_results();
    /** @brief Names and scores in the letterbox bars, or over the play area without bars. */
    void draw_hud();
    std::optional<Hud> hud_; // Built once the textures it borrows are loaded.
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
    ExplosionEffects explosions_;
    ImpactSounds impact_sounds_;
    float shake_ = 0.f; // seconds of screen shake left (our own player was hit)
    sf::Music music_;
    std::array<sf::Texture, 4> background_textures_;
    std::array<sf::Texture, 4> transition_background_textures_;
    sf::Clock frame_clock_;
    double automatic_background_x_ = 0.0;
    bool pause_menu_open_ = false;
    bool pause_held_ = false;
    bool results_open_ = false; // The lobby's game ended: the results screen is shown.
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
    std::map<int, ZonePattern> zones_; // attack patterns that are target zones (ZonePatterns.hpp)
    std::map<int, BeamPattern> beams_; // attack patterns that are beams (BeamPatterns.hpp)
    // What the server sent, drawn by engine entities. A player is two: the coloured halo,
    // drawn first, and the tooth. Others have one sprite, or none (area attacks: rings).
    struct Mirror {
        std::uint8_t kind = 0;
        std::uint8_t type = 0;
        int sprite = -1;
        int halo = -1;
        std::uint8_t hits = 0;  // the server's hit counter: a change is a new impact
        float flash = 0.f;      // seconds of hit flash left
        float blink = 0.f;      // seconds of blinking left (a player just hit)
        int look = -1;          // player: 0 intact, 1 damaged, 2 broken, 3 golden
    };
    void add_mirror(Mirror& mirror, const protocol::Entity& entity);
    void on_hit(Mirror& mirror, const protocol::Entity& entity);
    void set_player_look(Mirror& mirror, std::uint8_t hp);
    /** @brief Flashes, blinking and screen shake, a frame further. */
    void update_impacts(float elapsed_seconds);
    void draw_area_attacks();
    void show_deaths();
    std::map<std::uint16_t, Mirror> entities_; // network id -> engine entities
};

} // namespace ntype::client
