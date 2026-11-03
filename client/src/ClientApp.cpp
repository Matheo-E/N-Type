#include "ClientApp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

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
const sf::FloatRect kLobbyButton{350.f, 450.f, 280.f, 100.f};
const sf::FloatRect kMusicButton{650.f, 450.f, 280.f, 100.f};

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
    const std::string menu_path = asset_dir + "/ui/pause_menu_sheet.png";
    if (!player_texture_.loadFromFile(player_path))
        throw std::runtime_error("Unable to load player sprite: " + player_path);
    if (!menu_texture_.loadFromFile(menu_path))
        throw std::runtime_error("Unable to load pause menu sprite sheet: " + menu_path);
    load_background(0, 0);
    create_window();
    if (!transition_frame_.create(config::kWidth, config::kHeight))
        throw std::runtime_error("Unable to create the day/night transition buffer");
    transition_frame_.setSmooth(true);
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
    const float game_ratio = static_cast<float>(config::kWidth) / static_cast<float>(config::kHeight);
    sf::FloatRect viewport(0.f, 0.f, 1.f, 1.f);
    if (window_ratio > game_ratio) {
        viewport.width = game_ratio / window_ratio;
        viewport.left = (1.f - viewport.width) / 2.f;
    } else {
        viewport.height = window_ratio / game_ratio;
        viewport.top = (1.f - viewport.height) / 2.f;
    }
    sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(config::kWidth),
                               static_cast<float>(config::kHeight)));
    view.setViewport(viewport);
    window_.setView(view);
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
            it->second = Create_E(world_);
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
                                     const std::array<sf::Texture, 4>& textures) {
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
            sprite.setScale(scale, scale);
            sprite.setPosition(static_cast<float>(tile) * tile_width - offset, 0.f);
            target.draw(sprite);
        }
    }
}

/**
 * @brief Draws the current background and the optional day/night crossfade.
 *
 * The incoming four-layer scene is first composited at full opacity in an
 * off-screen texture. Blending that single finished image prevents transparent
 * foreground layers from fading at different apparent speeds.
 */
void ClientApp::draw_parallax() {
    draw_parallax_layers(window_, background_textures_);
    if (!ambience_transition_active_) return;

    transition_frame_.clear(sf::Color::Transparent);
    draw_parallax_layers(transition_frame_, transition_background_textures_);
    transition_frame_.display();

    const float smooth_progress =
        background::day_night_fade_progress(transition_elapsed_seconds_);
    sf::Sprite transition_sprite(transition_frame_.getTexture());
    transition_sprite.setColor(sf::Color(
        255, 255, 255, static_cast<sf::Uint8>(smooth_progress * 255.f + 0.5f)));
    window_.draw(transition_sprite);
}

/**
 * @brief Draws the pause controls by cropping the pause-menu sprite sheet.
 *
 * Pausing suppresses local input but networking continues, which keeps a
 * multiplayer client connected and allows other players to keep moving.
 */
void ClientApp::draw_pause_menu() {
    const auto draw_asset = [&](const sf::IntRect source, const sf::FloatRect target) {
        sf::Sprite sprite(menu_texture_);
        sprite.setTextureRect(source);
        sprite.setPosition(target.left, target.top);
        sprite.setScale(target.width / static_cast<float>(source.width),
                        target.height / static_cast<float>(source.height));
        window_.draw(sprite);
    };

    // The pause icon remains accessible while playing and closes the overlay too.
    draw_asset({390, 790, 158, 145}, kMenuButton);
    if (!pause_menu_open_) return;

    sf::RectangleShape shade({static_cast<float>(config::kWidth),
                              static_cast<float>(config::kHeight)});
    shade.setFillColor(sf::Color(5, 8, 30, 185));
    window_.draw(shade);
    sf::RectangleShape panel({640.f, 470.f});
    panel.setPosition(320.f, 125.f);
    panel.setFillColor(sf::Color(19, 22, 112, 235));
    panel.setOutlineThickness(4.f);
    panel.setOutlineColor(sf::Color(91, 196, 245));
    window_.draw(panel);

    draw_asset({1080, 250, 350, 130}, kContinueButton);
    draw_asset({380, 250, 350, 130}, kRestartButton);
    draw_asset({15, 250, 350, 130}, kLobbyButton);
    draw_asset(music_enabled_ ? sf::IntRect{380, 520, 350, 130}
                              : sf::IntRect{15, 520, 350, 130},
               kMusicButton);
    draw_asset({390, 790, 158, 145}, kMenuButton);
}

void ClientApp::render() {
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
