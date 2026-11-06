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
constexpr float kPlayerAnimationSpeed = 8.f;
constexpr int kPlayerAnimationFrames[] = {0, 1, 2, 1};
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

ClientApp::ClientApp(sf::RenderWindow& window, WindowState& window_state, const std::string& name,
                     const std::string& host, const std::uint16_t port,
                     const bool music_enabled)
    : name_(name), network_(host, port), window_(window), window_state_(window_state),
      music_enabled_(music_enabled) {
    const std::string asset_dir = NTYPE_ASSET_DIR;
    const std::string player_path = asset_dir + "/sprites/player/tooth.png";
    if (!player_texture_.loadFromFile(player_path))
        throw std::runtime_error("Unable to load player sprite: " + player_path);
    if (!load_ui_font(ui_font_)) throw std::runtime_error("Unable to load the UI font");
    if (!pause_gui_.buttons.loadFromFile(asset_dir + "/FreeGUI/png/Button.png") ||
        !pause_gui_.windows.loadFromFile(asset_dir + "/FreeGUI/png/Window.png"))
        throw std::runtime_error("Unable to load pause menu assets");
    pause_gui_.buttons.setSmooth(true);
    pause_gui_.windows.setSmooth(true);
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

void ClientApp::create_window() {
    window_state_.open(window_, "N-Type | Connecting");
    update_view();
}

void ClientApp::update_view() {
    const auto size = window_.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float window_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float game_ratio = static_cast<float>(config::kWidth) /
                             static_cast<float>(config::kHeight);
    sf::View view(sf::FloatRect(0.f, 0.f, static_cast<float>(config::kWidth),
                               static_cast<float>(config::kHeight)));
    // Fit the complete play area into the retained window, without cropping it.
    sf::FloatRect viewport{0.f, 0.f, 1.f, 1.f};
    if (window_ratio > game_ratio) {
        viewport.width = game_ratio / window_ratio;
        viewport.left = (1.f - viewport.width) / 2.f;
    } else {
        viewport.height = window_ratio / game_ratio;
        viewport.top = (1.f - viewport.height) / 2.f;
    }
    view.setViewport(viewport);
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
            window_state_.toggle_fullscreen(window_, "N-Type");
            update_view();
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
 * @brief Draws one complete Parallax background into a render target.
 *
 * Each layer uses only the time-based automatic distance multiplied by its
 * configured factor. Scrolling speed stays constant regardless of player movement.
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
        float offset = static_cast<float>(std::fmod(
            automatic_background_x_ * static_cast<double>(background::kParallaxFactors[layer]),
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

/** @brief Draws the pause controls using the shared FreeGUI skin. */
void ClientApp::draw_pause_menu() {
    const auto mouse = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
    const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
    pause_gui_.icon(window_, {3090, 315, 185, 195}, kMenuButton);
    if (!pause_menu_open_) return;

    sf::RectangleShape shade({static_cast<float>(config::kWidth),
                              static_cast<float>(config::kHeight)});
    shade.setFillColor(sf::Color(5, 8, 30, 185));
    window_.draw(shade);
    pause_gui_.panel(window_, ui_font_, "Pause", {320.f, 125.f, 640.f, 470.f});
    pause_gui_.button(window_, ui_font_, kContinueButton, "Continuer", mouse, pressed, true, 1, 32);
    pause_gui_.button(window_, ui_font_, kRestartButton, "Recommencer", mouse, pressed, true, 0, 32);
    pause_gui_.button(window_, ui_font_, kLobbyButton, "Lobby", mouse, pressed, true, 2, 24);
    pause_gui_.button(window_, ui_font_, kMusicButton,
                      music_enabled_ ? "Musique : ON" : "Musique : OFF", mouse, pressed,
                      true, music_enabled_ ? 1 : 2, 18);
    const bool sound_enabled = UiClickSound::instance().enabled();
    pause_gui_.button(window_, ui_font_, kClickSoundButton,
                      sound_enabled ? "Son : ON" : "Son : OFF", mouse, pressed,
                      true, sound_enabled ? 1 : 2, 20);
}

void ClientApp::render() {
    window_.setActive(true);
    update_view();
    sync_world();
    const float elapsed_seconds = std::min(frame_clock_.restart().asSeconds(), 0.1f);
    update_adventure(elapsed_seconds);
    automatic_background_x_ += elapsed_seconds * background::kAutomaticScrollSpeed;
    window_.clear(sf::Color::Black);
    draw_parallax();

    // The wings keep flapping even when server positions do not change.
    const float animation_time = animation_clock_.getElapsedTime().asSeconds();
    const int animation_step = static_cast<int>(animation_time * kPlayerAnimationSpeed) % 4;
    const int frame = kPlayerAnimationFrames[animation_step];
    for (const auto& [player_id, entity] : entities_) {
        const auto position = world_.Positions.at(entity).Xy;
        sf::Sprite tooth(player_texture_);
        tooth.setTextureRect({frame * kPlayerFrameSize, 0, kPlayerFrameSize, kPlayerFrameSize});
        const float scale = kRenderedPlayerSize / static_cast<float>(kPlayerFrameSize);
        tooth.setScale(scale, scale);
        const float offset = (kRenderedPlayerSize - config::kPlayerSize) / 2.f;
        tooth.setPosition(position.x - offset, position.y - offset);
        window_.draw(tooth);
    }
    draw_pause_menu();
    window_.display();
}

} // namespace ntype::client
