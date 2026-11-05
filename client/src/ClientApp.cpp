#include "ClientApp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/OpenGL.hpp>

#include "Factory.hpp"
#include "BackgroundConfig.hpp"
#include "GameConfig.hpp"
#include "Protocol.hpp"
#include "System.hpp"
#include "UiClickSound.hpp"

namespace ntype::client {

namespace {

const std::array<const char*, config::kMaxPlayers> kColorNames{"blue", "orange", "green", "pink"};

constexpr int kPlayerFrameSize = 724;
constexpr float kRenderedPlayerSize = 88.f;
constexpr float kCameraAnchorX = 320.f;
constexpr float kCameraResponsiveness = 10.f;
const sf::FloatRect kMenuButton{1195.f, 20.f, 65.f, 61.f};
const sf::FloatRect kContinueButton{430.f, 175.f, 420.f, 110.f};
const sf::FloatRect kRestartButton{430.f, 305.f, 420.f, 110.f};
const sf::FloatRect kLobbyButton{350.f, 450.f, 180.f, 100.f};
const sf::FloatRect kMusicButton{550.f, 450.f, 180.f, 100.f};
const sf::FloatRect kClickSoundButton{750.f, 450.f, 180.f, 100.f};

bool load_ui_font(sf::Font& font) {
    const std::array<const char*, 4> paths{
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
        "/usr/local/share/fonts/DejaVuSans.ttf", "DejaVuSans.ttf",
    };
    return std::any_of(paths.begin(), paths.end(), [&](const char* path) {
        return font.loadFromFile(path);
    });
}

constexpr std::uint8_t bit(const protocol::InputType key) {
    return static_cast<std::uint8_t>(key);
}

} // namespace

ClientApp::ClientApp(sf::RenderWindow& window, bool& fullscreen, const std::string& name,
                     const std::string& host, const std::uint16_t port,
                     const bool music_enabled)
    : name_(name), network_(host, port), window_(window), fullscreen_(fullscreen),
      music_enabled_(music_enabled) {
    const std::string asset_dir = NTYPE_ASSET_DIR;
    const std::string player_path = asset_dir + "/sprites/player/tooth.png";
    if (!player_texture_.loadFromFile(player_path))
        throw std::runtime_error("Unable to load player sprite: " + player_path);
    if (!load_ui_font(ui_font_)) throw std::runtime_error("Unable to load the UI font");
    load_background(0, 0);
    create_window();
    load_level_music(0);
    frame_clock_.restart();
}

/**
 * @brief Loads one set of four Parallax layers without changing adventure state.
 * @param textures Destination textures to replace.
 * @param level Zero-based adventure level.
 * @param ambience Zero for day, one for night.
 */
void ClientApp::load_background_layers(std::array<sf::Texture, 4>& textures,
                                       const std::size_t level,
                                       const std::size_t ambience) {
    const std::string asset_dir = NTYPE_ASSET_DIR;
    const background::Selection selection{level, ambience};
    for (std::size_t layer = 0; layer < textures.size(); ++layer) {
        const auto path = background::layer_path(asset_dir, selection, layer);
        if (!textures[layer].loadFromFile(path))
            throw std::runtime_error("Unable to load parallax layer: " + path);
        textures[layer].setSmooth(true);
    }
}

/**
 * @brief Loads the selected background and preloads its opposite ambience.
 * @param level Zero-based adventure level.
 * @param ambience Zero for day, one for night.
 */
void ClientApp::load_background(const std::size_t level, const std::size_t ambience) {
    load_background_layers(background_textures_, level, ambience);
    load_background_layers(transition_background_textures_, level,
                           (ambience + 1U) % background::kAmbienceCount);
    current_level_ = level;
    current_ambience_ = ambience;
    transition_ambience_ = (ambience + 1U) % background::kAmbienceCount;
    transition_elapsed_seconds_ = 0.f;
    ambience_transition_active_ = false;
}

/** @brief Replaces the looping soundtrack when the automatic level changes. */
void ClientApp::load_level_music(const std::size_t level) {
    const auto path = background::music_path(NTYPE_ASSET_DIR, {level, 0});
    music_.stop();
    if (!music_.openFromFile(path))
        throw std::runtime_error("Unable to load level music: " + path);
    music_.setLoop(true);
    music_.setVolume(40.f);
    if (music_enabled_) music_.play();
}

/**
 * @brief Advances the timed adventure and smoothly blends day with night.
 *
 * A change of level is kept separate and loaded directly so two different
 * worlds are never mixed. An ambience-only change uses the preloaded textures
 * for a six-second smoothstep crossfade.
 *
 * @param elapsed_seconds Duration of the last rendered frame.
 */
void ClientApp::update_adventure(const float elapsed_seconds) {
    adventure_elapsed_seconds_ += elapsed_seconds;
    const auto selection = background::selection_at_time(adventure_elapsed_seconds_);

    if (selection.level != current_level_) {
        load_background(selection.level, selection.ambience);
        load_level_music(selection.level);
        return;
    }

    if (ambience_transition_active_) {
        transition_elapsed_seconds_ += elapsed_seconds;
        if (transition_elapsed_seconds_ < background::kDayNightFadeDurationSeconds) return;

        for (std::size_t layer = 0; layer < background_textures_.size(); ++layer)
            background_textures_[layer].swap(transition_background_textures_[layer]);
        current_ambience_ = transition_ambience_;
        transition_elapsed_seconds_ = 0.f;
        ambience_transition_active_ = false;
        return;
    }

    if (selection.ambience == current_ambience_) return;
    transition_ambience_ = selection.ambience;
    transition_elapsed_seconds_ = 0.f;
    ambience_transition_active_ = true;
}

void ClientApp::create_window(const bool recreate) {
    if (!window_.isOpen() || recreate) {
        if (fullscreen_) {
            window_.create(sf::VideoMode::getDesktopMode(), "N-Type | Connecting",
                           sf::Style::Fullscreen);
        } else {
            window_.create(sf::VideoMode(config::kWidth, config::kHeight),
                           "N-Type | Connecting", sf::Style::Default);
        }
    } else {
        window_.setTitle("N-Type | Connecting");
        if (!fullscreen_) window_.setSize({config::kWidth, config::kHeight});
    }
    window_.setFramerateLimit(60);
    update_view();
}

void ClientApp::update_view() {
    const auto size = window_.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float window_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float visible_width = static_cast<float>(config::kHeight) * window_ratio;
    sf::View view(sf::FloatRect((static_cast<float>(config::kWidth) - visible_width) / 2.f,
                               0.f, visible_width, static_cast<float>(config::kHeight)));
    view.setViewport({0.f, 0.f, 1.f, 1.f});
    window_.setView(view);
    window_.setActive(true);
    glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
}

GameExitAction ClientApp::run() {
    while (window_.isOpen() && session_active_) {
        poll_events();
        if (!window_.isOpen() || !session_active_) {
            break;
        }
        network_.update(pause_menu_open_ ? 0 : read_input());
        std::string title = "N-Type | " + name_ + " | " + network_.status();
        if (const auto id = network_.id()) {
            title += std::string(" (") + kColorNames[*id - 1U] + ")";
        }
        window_.setTitle(title + " | Arrows/WASD/ZQSD | F11: fullscreen | Esc: menu");
        render();
    }
    network_.disconnect();
    music_.stop();
    return exit_action_;
}

void ClientApp::poll_events() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            exit_action_ = GameExitAction::Quit;
            session_active_ = false;
            window_.close();
            return;
        }
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            pause_menu_open_ = !pause_menu_open_;
        }
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
            fullscreen_ = !fullscreen_;
            create_window(true);
        }
        if (event.type == sf::Event::Resized) update_view();
        if (event.type == sf::Event::MouseButtonPressed &&
            event.mouseButton.button == sf::Mouse::Left) {
            const auto point = window_.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
            if (kMenuButton.contains(point)) {
                UiClickSound::instance().play();
                pause_menu_open_ = !pause_menu_open_;
            } else if (pause_menu_open_ && kContinueButton.contains(point)) {
                UiClickSound::instance().play();
                pause_menu_open_ = false;
            } else if (pause_menu_open_ && kRestartButton.contains(point)) {
                UiClickSound::instance().play();
                exit_action_ = GameExitAction::Restart;
                session_active_ = false;
                return;
            } else if (pause_menu_open_ && kLobbyButton.contains(point)) {
                UiClickSound::instance().play();
                exit_action_ = GameExitAction::Lobby;
                session_active_ = false;
                return;
            } else if (pause_menu_open_ && kMusicButton.contains(point)) {
                UiClickSound::instance().play();
                music_enabled_ = !music_enabled_;
                if (music_enabled_)
                    music_.play();
                else
                    music_.pause();
            } else if (pause_menu_open_ && kClickSoundButton.contains(point)) {
                auto& click_sound = UiClickSound::instance();
                click_sound.set_enabled(!click_sound.enabled());
            }
        }
    }
}

std::uint8_t ClientApp::read_input() const {
    if (!window_.hasFocus() || pause_menu_open_) {
        return 0;
    }
    using Key = sf::Keyboard::Key;
    using protocol::InputType;
    const auto pressed = [](const Key key) { return sf::Keyboard::isKeyPressed(key); };
    std::uint8_t bits = 0;
    if (pressed(Key::Up) || pressed(Key::W) || pressed(Key::Z)) {
        bits |= bit(InputType::Up);
    }
    if (pressed(Key::Down) || pressed(Key::S)) {
        bits |= bit(InputType::Down);
    }
    if (pressed(Key::Left) || pressed(Key::A) || pressed(Key::Q)) {
        bits |= bit(InputType::Left);
    }
    if (pressed(Key::Right) || pressed(Key::D)) {
        bits |= bit(InputType::Right);
    }
    if (pressed(Key::Space)) {
        bits |= bit(InputType::Shoot);
    }
    return bits;
}

void ClientApp::sync_world() {
    const auto& players = network_.players();
    // Only players in the latest snapshot keep an entity.
    for (auto it = entities_.begin(); it != entities_.end();) {
        const auto id = it->first;
        if (std::none_of(players.begin(), players.end(), [id](const auto& player) { return player.id == id; })) {
            world_.Positions.erase(it->second);
            world_.Rectangles.erase(it->second);
            previous_positions_.erase(id);
            last_movement_times_.erase(id);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& player : players) {
        auto [it, added] = entities_.try_emplace(player.id, 0);
        if (added) {
            it->second = CreateE(world_);
        }
        // The server is authoritative: positions are copied, never simulated here.
        world_.Positions[it->second].Xy = {static_cast<float>(player.x), static_cast<float>(player.y)};
    }
}

/**
 * @brief Smoothly follows the local player while leaving server coordinates untouched.
 * @param elapsed_seconds Time elapsed since the previous rendered frame.
 */
void ClientApp::update_camera(const float elapsed_seconds) {
    const auto local_id = network_.id();
    if (!local_id) return;
    const auto entity = entities_.find(*local_id);
    if (entity == entities_.end()) return;

    const float player_x = world_.Positions.at(entity->second).Xy.x;
    const float target = std::max(0.f, player_x - kCameraAnchorX);
    if (!camera_initialized_) {
        camera_x_ = target;
        camera_initialized_ = true;
        return;
    }
    const float blend = std::min(1.f, elapsed_seconds * kCameraResponsiveness);
    camera_x_ += (target - camera_x_) * blend;
}

/**
 * @brief Draws one complete Parallax background into a render target.
 *
 * Each layer combines the player camera with a time-based automatic distance,
 * then multiplies that total by its configured factor. The background therefore
 * keeps moving while the player is stationary without changing world positions.
 * Two copies are sufficient because every source image is wider than the viewport
 * after scaling; a third copy is drawn defensively for future narrower assets.
 *
 * @param target Window or off-screen transition buffer receiving the background.
 * @param textures Four layers, ordered from distant scenery to ground.
 */
void ClientApp::draw_parallax_layers(sf::RenderTarget& target,
                                     const std::array<sf::Texture, 4>& textures,
                                     const sf::Uint8 opacity) {
    for (std::size_t layer = 0; layer < textures.size(); ++layer) {
        const auto texture_size = textures[layer].getSize();
        const float scale = static_cast<float>(config::kHeight) /
                            static_cast<float>(texture_size.y);
        const float tile_width = static_cast<float>(texture_size.x) * scale;
        const double scrolling_x =
            static_cast<double>(camera_x_) + automatic_background_x_;
        float offset = static_cast<float>(std::fmod(
            scrolling_x * static_cast<double>(background::kParallaxFactors[layer]),
            static_cast<double>(tile_width)));
        if (offset < 0.f) offset += tile_width;

        for (int tile = -1; tile <= 1; ++tile) {
            sf::Sprite sprite(textures[layer]);
            sprite.setColor(sf::Color(255, 255, 255, opacity));
            sprite.setScale(scale, scale);
            sprite.setPosition(static_cast<float>(tile) * tile_width - offset, 0.f);
            target.draw(sprite);
        }
    }
}

/**
 * @brief Draws the current background and the optional day/night crossfade.
 *
 * The incoming layers are blended directly into the window. Keeping rendering
 * on one target prevents an off-screen OpenGL context from leaking its 1280x720
 * viewport into a larger fullscreen or maximized window.
 */
void ClientApp::draw_parallax() {
    draw_parallax_layers(window_, background_textures_);
    if (!ambience_transition_active_) return;

    const float smooth_progress =
        background::day_night_fade_progress(transition_elapsed_seconds_);
    draw_parallax_layers(
        window_, transition_background_textures_,
        static_cast<sf::Uint8>(smooth_progress * 255.f + 0.5f));
}

/** @brief Draws the pause controls from SFML primitives and live text. */
void ClientApp::draw_pause_menu() {
    const sf::Vector2f mouse = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
    const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    const auto draw_button = [&](const sf::FloatRect& bounds, const std::string& label,
                                 const bool enabled = true, const bool toggle = false,
                                 const bool active = true) {
        sf::Color fill = enabled ? sf::Color(27, 78, 119) : sf::Color(55, 64, 74);
        if (toggle) fill = active ? sf::Color(42, 132, 104) : sf::Color(55, 64, 74);
        if (enabled && bounds.contains(mouse))
            fill = pressed ? sf::Color(25, 68, 103)
                           : (toggle && active ? sf::Color(52, 151, 119)
                                               : sf::Color(76, 88, 103));
        sf::RectangleShape button({bounds.width, bounds.height});
        button.setPosition(bounds.left, bounds.top);
        button.setFillColor(fill);
        button.setOutlineThickness(2.f);
        button.setOutlineColor(sf::Color(91, 145, 183));
        window_.draw(button);
        sf::Text text(label, ui_font_, bounds.height < 70.f ? 17U : 30U);
        text.setFillColor(enabled ? sf::Color::White : sf::Color(155, 165, 175));
        const auto text_bounds = text.getLocalBounds();
        text.setPosition(bounds.left + (bounds.width - text_bounds.width) / 2.f - text_bounds.left,
                         bounds.top + (bounds.height - text_bounds.height) / 2.f - text_bounds.top - 2.f);
        window_.draw(text);
    };

    sf::RectangleShape menu_button({kMenuButton.width, kMenuButton.height});
    menu_button.setPosition(kMenuButton.left, kMenuButton.top);
    menu_button.setFillColor(kMenuButton.contains(mouse) ? sf::Color(36, 99, 148)
                                                         : sf::Color(15, 49, 78, 230));
    menu_button.setOutlineThickness(1.f);
    menu_button.setOutlineColor(sf::Color(110, 164, 202));
    window_.draw(menu_button);
    for (int line = 0; line < 3; ++line) {
        sf::RectangleShape bar({27.f, 3.f});
        bar.setPosition(kMenuButton.left + 19.f,
                        kMenuButton.top + 19.f + static_cast<float>(line) * 10.f);
        bar.setFillColor(sf::Color::White);
        window_.draw(bar);
    }
    if (!pause_menu_open_) return;

    sf::RectangleShape shade({static_cast<float>(config::kWidth),
                              static_cast<float>(config::kHeight)});
    shade.setFillColor(sf::Color(5, 8, 30, 185));
    window_.draw(shade);
    sf::RectangleShape panel({640.f, 470.f});
    panel.setPosition(320.f, 125.f);
    panel.setFillColor(sf::Color(12, 34, 58, 245));
    panel.setOutlineThickness(2.f);
    panel.setOutlineColor(sf::Color(91, 145, 183));
    window_.draw(panel);
    draw_button(kContinueButton, "Continue");
    draw_button(kRestartButton, "Restart");
    draw_button(kLobbyButton, "Lobby");
    draw_button(kMusicButton, "Musique", true, true, music_enabled_);
    draw_button(kClickSoundButton, "Son", true, true, UiClickSound::instance().enabled());
}

void ClientApp::render() {
    window_.setActive(true);
    update_view();
    sync_world();
    const float elapsed_seconds = std::min(frame_clock_.restart().asSeconds(), 0.1f);
    update_adventure(elapsed_seconds);
    update_camera(elapsed_seconds);
    automatic_background_x_ += elapsed_seconds * background::kAutomaticScrollSpeed;
    window_.clear(sf::Color::Black);
    draw_parallax();

    const float animation_time = animation_clock_.getElapsedTime().asSeconds();
    for (const auto& [player_id, entity] : entities_) {
        const auto position = world_.Positions.at(entity).Xy;
        const auto previous = previous_positions_.find(player_id);
        const bool position_changed = previous != previous_positions_.end() &&
            (std::abs(position.x - previous->second.x) > 0.05f ||
             std::abs(position.y - previous->second.y) > 0.05f);
        if (position_changed) last_movement_times_[player_id] = animation_time;
        const auto last_movement = last_movement_times_.find(player_id);
        const bool moving = last_movement != last_movement_times_.end() &&
                            animation_time - last_movement->second < 0.12f;
        const int frame = moving ? 1 + static_cast<int>(animation_time * 8.f) % 2 : 0;

        sf::Sprite tooth(player_texture_);
        tooth.setTextureRect({frame * kPlayerFrameSize, 0, kPlayerFrameSize, kPlayerFrameSize});
        const float scale = kRenderedPlayerSize / static_cast<float>(kPlayerFrameSize);
        tooth.setScale(scale, scale);
        const float offset = (kRenderedPlayerSize - config::kPlayerSize) / 2.f;
        tooth.setPosition(position.x - camera_x_ - offset, position.y - offset);
        window_.draw(tooth);
        previous_positions_[player_id] = position;
    }
    draw_pause_menu();
    window_.display();
}

} // namespace ntype::client
