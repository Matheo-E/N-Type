#include "ClientApp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/OpenGL.hpp>

#include "Factory.hpp"
#include "Loader.hpp"
#include "BackgroundConfig.hpp"
#include "GameConfig.hpp"
#include "Protocol.hpp"
#include "PlayerProfile.hpp"
#include "System.hpp"
#include "UiClickSound.hpp"
#include "UiFont.hpp"
#include "Hud.hpp"
#include "PlayerColors.hpp"
#include "ResultsScreen.hpp"

namespace ntype::client {

namespace {


constexpr int kPlayerTexture = 1;  // id of sprites/player/tooth.png in assets/data/textures.json
constexpr int kGoldenTexture = 32; // same three animation frames as the normal tooth
constexpr int kHaloTexture = 21;   // id of sprites/player/tooth_halo.png (tools/make_tooth_halo.py)
// Halo frames are 4x smaller than the tooth's, plus 28 pixels of margin on each side.
constexpr int kHaloFrameSize = 237;
constexpr float kHaloShrink = 4.f;
constexpr sf::Uint8 kHaloOpacity = 235;
constexpr int kPlayerFrameSize = 724;
constexpr float kRenderedPlayerSize = 88.f;
// The tooth as its health goes down: intact, damaged, broken (1 HP left), and their halos
// (ids in assets/data/textures.json; halos made by tools/make_tooth_halo.py).
constexpr int kToothLooks[] = {kPlayerTexture, 23, 24};
constexpr int kHaloLooks[] = {kHaloTexture, 25, 26};
// Impact feedback.
constexpr float kEnemyFlashSeconds = 0.08f;
constexpr float kPlayerFlashSeconds = 0.15f;
constexpr float kPlayerBlinkSeconds = 1.0f; // as long as the engine's protection after a hit
constexpr float kShakeSeconds = 0.25f;
constexpr float kShakePixels = 7.f;
const sf::FloatRect kMenuButton{1195.f, 20.f, 65.f, 61.f};
const sf::FloatRect kContinueButton{430.f, 185.f, 420.f, 110.f};
const sf::FloatRect kLobbyInfo{340.f, 315.f, 600.f, 110.f};
const sf::FloatRect kLobbyButton{350.f, 450.f, 180.f, 100.f};
const sf::FloatRect kMusicButton{550.f, 450.f, 180.f, 100.f};
const sf::FloatRect kControlsButton{480.f, 562.f, 320.f, 56.f};
const sf::FloatRect kClickSoundButton{750.f, 450.f, 180.f, 100.f};


} // namespace

ClientApp::ClientApp(sf::RenderWindow& window, WindowState& window_state, ClientNetwork& network,
                     const bool music_enabled)
    : network_(network), window_(window), window_state_(window_state),
      music_enabled_(music_enabled) {
    const std::string asset_dir = NTYPE_ASSET_DIR;
    if (!LoadTextures(world_, asset_dir))
        throw std::runtime_error("Unable to load the textures listed in assets/data/textures.json");
    // The looks of what the server sends: enemy types, shots (attack patterns) and items.
    const std::string data_dir = asset_dir + "/data/";
    if (!LoadEnemies(world_, data_dir + "enemies.json") ||
        !LoadAttackPatterns(world_, data_dir + "attack_patterns.json") ||
        !LoadItems(world_, data_dir + "items.json"))
        throw std::runtime_error("Unable to load the game data in " + data_dir);
    if (!load_ui_font(ui_font_)) throw std::runtime_error("Unable to load the UI font");
    if (!pause_gui_.buttons.loadFromFile(asset_dir + "/FreeGUI/png/Button.png") ||
        !pause_gui_.windows.loadFromFile(asset_dir + "/FreeGUI/png/Window.png"))
        throw std::runtime_error("Unable to load pause menu assets");
    pause_gui_.buttons.setSmooth(true);
    pause_gui_.windows.setSmooth(true);
    hud_.emplace(ui_font_, pause_gui_.windows, world_.Textures.at(kPlayerTexture).Image, asset_dir);
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
    auto selection = background::selection_at_time(adventure_elapsed_seconds_);
    // The server decides the level; day and night still follow the clock.
    if (const auto& lobby = network_.lobby(); lobby && lobby->level > 0)
        selection.level = std::min<std::size_t>(lobby->level - 1U, background::kLevelCount - 1U);

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
        // The lobby decides when the game ends; the session itself stays open.
        if (network_.phase() != Phase::InLobby) {
            exit_action_ = GameExitAction::Lobby; // Connection lost, or removed from the lobby.
            break;
        }
        const auto& lobby = *network_.lobby();
        if (lobby.state != protocol::LobbyPhase::Playing) {
            if (lobby.result == protocol::GameResult::None) {
                exit_action_ = GameExitAction::WaitingRoom; // stopped without a result
                break;
            }
            if (!results_open_) {
                results_open_ = true;
                pause_menu_open_ = false;
                controls_menu_.open = false;
            }
        } else if (results_open_) {
            results_open_ = false; // The leader started again: straight into the new game.
        }
        std::string title = "N-Type | " + network_.name() + " | " + lobby.name + " (" + lobby.code + ")";
        if (lobby.yourSlot >= 1 && lobby.yourSlot <= config::kMaxPlayers) {
            title += std::string(" | ") + kPlayerColorNames[lobby.yourSlot - 1U];
        }
        if (network_.is_spectator()) title += " | spectator";
        window_.setTitle(title + " | Arrows/WASD/ZQSD | F11: fullscreen | Esc: menu");
        render();
    }
    music_.stop();
    return exit_action_;
}

void ClientApp::poll_events() {
    sf::Event event{};
    while (window_.pollEvent(event)) {
        sf::Event released = event;
        if (event.type == sf::Event::KeyReleased) released.type = sf::Event::KeyPressed;
        if (event.type == sf::Event::MouseButtonReleased) released.type = sf::Event::MouseButtonPressed;
        if (event.type == sf::Event::JoystickButtonReleased) released.type = sf::Event::JoystickButtonPressed;
        if (event.type == sf::Event::LostFocus ||
            (released.type != event.type && PlayerProfile::shared().pause_event(released))) pause_held_ = false;
        if (controls_menu_.open && controls_menu_.handle_event(window_, event)) {
            continue;
        }
        if (event.type == sf::Event::Closed) {
            exit_action_ = GameExitAction::Quit;
            session_active_ = false;
            window_.close();
            return;
        }
        if (results_open_) {
            // The results screen only leaves for the waiting room (and keeps F11, resizing).
            const bool key = event.type == sf::Event::KeyPressed &&
                             (event.key.code == sf::Keyboard::Return || event.key.code == sf::Keyboard::Escape);
            const bool click = event.type == sf::Event::MouseButtonPressed &&
                               event.mouseButton.button == sf::Mouse::Left &&
                               kResultsContinue.contains(window_.mapPixelToCoords(
                                   {event.mouseButton.x, event.mouseButton.y}));
            if (key || click) {
                UiClickSound::instance().play();
                exit_action_ = GameExitAction::WaitingRoom;
                session_active_ = false;
                return;
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
                window_state_.toggle_fullscreen(window_, "N-Type");
                update_view();
            }
            if (event.type == sf::Event::Resized) update_view();
            continue;
        }
        // Left clicks on an open pause menu operate its buttons.
        const bool menu_click = pause_menu_open_ && event.type == sf::Event::MouseButtonPressed &&
                                event.mouseButton.button == sf::Mouse::Left;
        if (!menu_click && window_.hasFocus() && PlayerProfile::shared().pause_event(event)) {
            if (!pause_held_) pause_menu_open_ = !pause_menu_open_;
            pause_held_ = true;
            continue;
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
            } else if (pause_menu_open_ && kControlsButton.contains(point)) {
                UiClickSound::instance().play();
                controls_menu_.open = true;
            } else if (pause_menu_open_ && kContinueButton.contains(point)) {
                UiClickSound::instance().play();
                pause_menu_open_ = false;
            } else if (pause_menu_open_ && kLobbyButton.contains(point)) {
                UiClickSound::instance().play();
                network_.leave();
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
    // Spectators watch: they join as players at the next level.
    if (!window_.hasFocus() || pause_menu_open_ || results_open_ || network_.is_spectator()) {
        return 0;
    }
    const auto& profile = PlayerProfile::shared();
    ControlState state;
    for (int key = 0; key < sf::Keyboard::KeyCount; ++key)
        state.keys[static_cast<std::size_t>(key)] = sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(key));
    if (profile.device() == ControlDevice::Mouse) {
        for (int button = 0; button < sf::Mouse::ButtonCount; ++button)
            state.mouse[static_cast<std::size_t>(button)] = sf::Mouse::isButtonPressed(static_cast<sf::Mouse::Button>(button));
        const auto cursor = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        for (const auto& player : network_.players()) {
            if (network_.lobby() && player.id == network_.lobby()->yourSlot) {
                const float dx = cursor.x - (static_cast<float>(player.x) + config::kPlayerSize / 2.f);
                const float dy = cursor.y - (static_cast<float>(player.y) + config::kPlayerSize / 2.f);
                state.x = std::abs(dx) > 12.f ? dx : 0;
                state.y = std::abs(dy) > 12.f ? dy : 0;
            }
        }
    } else if (profile.device() == ControlDevice::Gamepad) {
        for (unsigned joystick = 0; joystick < sf::Joystick::Count; ++joystick) {
            if (!sf::Joystick::isConnected(joystick)) continue;
            for (unsigned button = 0; button < sf::Joystick::getButtonCount(joystick); ++button)
                state.buttons[button] = sf::Joystick::isButtonPressed(joystick, button);
            const auto axis = [joystick](sf::Joystick::Axis primary, sf::Joystick::Axis dpad) {
                float value = sf::Joystick::hasAxis(joystick, primary) ? sf::Joystick::getAxisPosition(joystick, primary) : 0.f;
                if (std::abs(value) <= 25.f && sf::Joystick::hasAxis(joystick, dpad))
                    value = sf::Joystick::getAxisPosition(joystick, dpad);
                return std::abs(value) > 25.f ? value : 0.f;
            };
            state.x = axis(sf::Joystick::X, sf::Joystick::PovX);
            // SFML's vertical D-pad axis is positive upwards, unlike the stick Y axis.
            const float y = sf::Joystick::hasAxis(joystick, sf::Joystick::Y) ? sf::Joystick::getAxisPosition(joystick, sf::Joystick::Y) : 0.f;
            state.y = std::abs(y) > 25.f ? y : sf::Joystick::hasAxis(joystick, sf::Joystick::PovY)
                ? -sf::Joystick::getAxisPosition(joystick, sf::Joystick::PovY) : 0.f;
            if (std::abs(state.y) <= 25.f) state.y = 0;
            break;
        }
    }
    return profile.input(state);
}

void ClientApp::sync_world() {
    const auto& entities = network_.entities();
    std::map<std::uint16_t, const protocol::Entity*> current;
    for (const auto& entity : entities) current[entity.id] = &entity;
    // Only what the latest frame holds keeps its sprites; an id that now means
    // something else (another kind or type) is rebuilt.
    for (auto it = entities_.begin(); it != entities_.end();) {
        const auto found = current.find(it->first);
        if (found == current.end() || found->second->kind != it->second.kind ||
            (it->second.kind != config::kPlayerKind && found->second->type != it->second.type)) {
            if (it->second.halo >= 0) world_.Destroy(it->second.halo);
            if (it->second.sprite >= 0) world_.Destroy(it->second.sprite);
            it = entities_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& entity : entities) {
        auto [it, added] = entities_.try_emplace(entity.id);
        if (added) {
            add_mirror(it->second, entity);
        } else if (entity.hits != it->second.hits) {
            on_hit(it->second, entity);
        }
        it->second.hits = entity.hits;
        it->second.type = entity.type;
        if (entity.kind == config::kPlayerKind) set_player_look(it->second, entity.hp);
        // The server is authoritative: positions are copied, never simulated here. It gives
        // a player's top-left corner and the centre of everything else; the engine draws
        // sprites around their centre.
        sf::Vector2f centre{static_cast<float>(entity.x), static_cast<float>(entity.y)};
        if (entity.kind == config::kPlayerKind) centre += {config::kPlayerSize / 2.f, config::kPlayerSize / 2.f};
        for (const int part : {it->second.halo, it->second.sprite})
            if (part >= 0) world_.Positions[part].Xy = centre;
    }
}

void ClientApp::add_mirror(Mirror& mirror, const protocol::Entity& entity) {
    using protocol::EntityKind;
    mirror.kind = entity.kind;
    mirror.type = entity.type;
    switch (static_cast<EntityKind>(entity.kind)) {
    case EntityKind::Player: {
        const float scale = kRenderedPlayerSize / static_cast<float>(kPlayerFrameSize);
        // Same Z for both: the engine breaks ties by entity id, so the halo, created
        // first, is painted behind the tooth.
        mirror.halo = CreateE(world_);
        ESprite& halo = world_.Sprites[mirror.halo];
        halo.TextureId = kHaloTexture;
        halo.Source = {0, 0, kHaloFrameSize, kHaloFrameSize};
        halo.Scale = {scale * kHaloShrink, scale * kHaloShrink};
        halo.Color = player_color(entity.id);
        halo.Color.a = kHaloOpacity;

        mirror.sprite = CreateE(world_);
        ESprite& tooth = world_.Sprites[mirror.sprite];
        tooth.TextureId = kPlayerTexture;
        tooth.Source = {0, 0, kPlayerFrameSize, kPlayerFrameSize};
        tooth.Scale = {scale, scale};

        // The tooth sheet holds three wing frames; they flap even when the player stands
        // still. Both entities start together, so the halo follows the wings.
        for (const int part : {mirror.halo, mirror.sprite}) {
            world_.ZIndexes[part].Z = ZPlayer;
            world_.Animations[part].Frames = {0, 1, 2, 1};
            world_.Animations[part].FrameTime = 0.125f;
        }
        return;
    }
    case EntityKind::Enemy:
        mirror.sprite = CreateE(world_);
        AddEnemyLook(world_, mirror.sprite, entity.type);
        return;
    case EntityKind::PlayerShot:
    case EntityKind::EnemyShot:
        mirror.sprite = CreateE(world_);
        if (!AddShotLook(world_, mirror.sprite, entity.type)) {
            world_.Destroy(mirror.sprite); // no sprite: an area attack, drawn as a ring
            mirror.sprite = -1;
        }
        return;
    case EntityKind::Item:
        mirror.sprite = CreateE(world_);
        AddItemLook(world_, mirror.sprite, entity.type);
        return;
    }
}

/** @brief A new hit on something already shown: its flash, its sound, maybe a shake. */
void ClientApp::on_hit(Mirror& mirror, const protocol::Entity& entity) {
    if (entity.kind == config::kPlayerKind) {
        mirror.flash = kPlayerFlashSeconds;
        mirror.blink = kPlayerBlinkSeconds;
        impact_sounds_.player_hit();
        const auto& lobby = network_.lobby();
        if (lobby && entity.id == lobby->yourSlot) shake_ = kShakeSeconds;
    } else if (entity.kind == static_cast<std::uint8_t>(protocol::EntityKind::Enemy)) {
        mirror.flash = kEnemyFlashSeconds;
        impact_sounds_.enemy_hit();
    }
}

/** @brief The tooth wears its damage: intact at full health, damaged, broken at 1 HP of 3. */
void ClientApp::set_player_look(Mirror& mirror, const std::uint8_t hp) {
    const int look = mirror.type == protocol::kGoldenPlayer ? 3 : hp >= 100 ? 0 : hp > 34 ? 1 : 2;
    if (look == mirror.look || mirror.sprite < 0 || mirror.halo < 0) return;
    mirror.look = look;
    world_.Sprites[mirror.sprite].TextureId = look == 3 ? kGoldenTexture : kToothLooks[look];
    world_.Sprites[mirror.halo].TextureId = look == 3 ? kHaloTexture : kHaloLooks[look];
}

void ClientApp::update_impacts(const float elapsed_seconds) {
    for (auto& [id, mirror] : entities_) {
        mirror.flash = std::max(0.f, mirror.flash - elapsed_seconds);
        mirror.blink = std::max(0.f, mirror.blink - elapsed_seconds);
        if (mirror.sprite < 0) continue;
        ESprite& look = world_.Sprites[mirror.sprite];
        if (mirror.kind != config::kPlayerKind) {
            // An enemy lights up white: its image again, added on top.
            const auto strength = static_cast<sf::Uint8>(230.f * mirror.flash / kEnemyFlashSeconds);
            look.Flash = mirror.flash > 0.f ? sf::Color(255, 255, 255, strength) : sf::Color::Transparent;
            continue;
        }
        // A hit player turns red (a tint: added red would not show on a white tooth),
        // then blinks while the engine protects it.
        const float red = mirror.flash / kPlayerFlashSeconds; // 1 just hit, 0 back to normal
        const auto others = static_cast<sf::Uint8>(255.f - 185.f * red);
        const bool dim = mirror.blink > 0.f && static_cast<int>(mirror.blink * 12.f) % 2 == 0;
        look.Color = sf::Color(255, others, others, dim ? 70 : 255);
        if (mirror.halo >= 0) world_.Sprites[mirror.halo].Color.a = dim ? 60 : kHaloOpacity;
    }
    shake_ = std::max(0.f, shake_ - elapsed_seconds);
    if (shake_ > 0.f) {
        // Our player was hit: the whole game view jolts, less and less.
        const float amplitude = kShakePixels * shake_ / kShakeSeconds;
        sf::View view = window_.getView();
        view.move(amplitude * std::sin(shake_ * 97.f), amplitude * std::cos(shake_ * 71.f));
        window_.setView(view);
    }
}

/** @brief Area attacks have no sprite: a translucent ring shows where they hurt. */
void ClientApp::draw_area_attacks() {
    using protocol::EntityKind;
    for (const auto& entity : network_.entities()) {
        const auto kind = static_cast<EntityKind>(entity.kind);
        if (kind != EntityKind::EnemyShot && kind != EntityKind::PlayerShot) continue;
        const auto pattern = world_.AttackPatterns.find(entity.type);
        if (pattern == world_.AttackPatterns.end() || pattern->second.TextureId >= 0) continue;
        const float radius = pattern->second.Radius;
        const bool warning = pattern->second.TargetZone && entity.hp < 100;
        const sf::Color colour = warning ? sf::Color(255, 195, 50) : sf::Color(255, 70, 70);
        sf::CircleShape ring(radius, 48);
        ring.setOrigin(radius, radius);
        ring.setPosition(static_cast<float>(entity.x), static_cast<float>(entity.y));
        ring.setFillColor(sf::Color(colour.r, colour.g, colour.b, warning ? 25 : 65));
        ring.setOutlineColor(sf::Color(colour.r, colour.g, colour.b, 220));
        ring.setOutlineThickness(-4.f);
        window_.draw(ring);
        if (warning) {
            const float progress = static_cast<float>(entity.hp) / 100.f;
            sf::CircleShape timer(radius * progress, 48);
            timer.setOrigin(radius * progress, radius * progress);
            timer.setPosition(ring.getPosition());
            timer.setFillColor(sf::Color(255, 195, 50, 70));
            window_.draw(timer);
        }
    }
}

/** @brief An explosion where each enemy or player died, sized to what died. */
void ClientApp::show_deaths() {
    for (const protocol::Entity& body : network_.take_deaths()) {
        sf::Vector2f centre{static_cast<float>(body.x), static_cast<float>(body.y)};
        float scale = 1.f;
        if (body.kind == config::kPlayerKind) {
            centre += {config::kPlayerSize / 2.f, config::kPlayerSize / 2.f};
        } else if (const auto type = world_.EnemyTypes.find(body.type); type != world_.EnemyTypes.end()) {
            scale = std::clamp(type->second.Radius / 28.f, 0.8f, 3.2f);
        }
        explosions_.spawn(centre, scale);
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

/** @brief Hands the HUD the play area as letterboxed in the window, in pixels. */
void ClientApp::draw_hud() {
    const auto& lobby = network_.lobby();
    if (!lobby || !hud_) return;
    const sf::Vector2f size(window_.getSize());
    const sf::FloatRect viewport = window_.getView().getViewport();
    const sf::FloatRect play{viewport.left * size.x, viewport.top * size.y,
                             viewport.width * size.x, viewport.height * size.y};
    const float unit = play.height / static_cast<float>(config::kHeight);
    // The boss of the level, if one is on screen.
    std::optional<Hud::Boss> boss;
    for (const auto& entity : network_.entities()) {
        if (entity.kind != static_cast<std::uint8_t>(protocol::EntityKind::Enemy)) continue;
        const auto type = world_.EnemyTypes.find(entity.type);
        if (type != world_.EnemyTypes.end() && type->second.Boss) boss = Hud::Boss{type->second.Name, entity.hp};
    }
    hud_->draw(window_, *lobby, network_.entities(), boss, play, play.left + kMenuButton.left * unit);
}

void ClientApp::draw_results() {
    const auto& lobby = network_.lobby();
    if (!lobby) return;
    const auto mouse = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
    draw_results_screen(window_, pause_gui_, ui_font_, *lobby, mouse, sf::Mouse::isButtonPressed(sf::Mouse::Left));
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
    pause_gui_.panel(window_, ui_font_, "Pause", {320.f, 125.f, 640.f, 515.f});
    pause_gui_.button(window_, ui_font_, kContinueButton, "Continuer", mouse, pressed, true, 1, 32);
    if (const auto& lobby = network_.lobby()) {
        pause_gui_.centered(window_, ui_font_, "Lobby : " + lobby->name,
                            {kLobbyInfo.left, kLobbyInfo.top + 10.f, kLobbyInfo.width, 40.f}, 26);
        pause_gui_.centered(window_, ui_font_, "Code : " + lobby->code,
                            {kLobbyInfo.left, kLobbyInfo.top + 58.f, kLobbyInfo.width, 34.f}, 22, style::muted);
    }
    pause_gui_.button(window_, ui_font_, kLobbyButton, "Lobby", mouse, pressed, true, 2, 24);
    pause_gui_.button(window_, ui_font_, kMusicButton,
                      music_enabled_ ? "Musique : ON" : "Musique : OFF", mouse, pressed,
                      true, music_enabled_ ? 1 : 2, 18);
    const bool sound_enabled = UiClickSound::instance().enabled();
    pause_gui_.button(window_, ui_font_, kClickSoundButton,
                      sound_enabled ? "Son : ON" : "Son : OFF", mouse, pressed,
                      true, sound_enabled ? 1 : 2, 20);
    pause_gui_.button(window_, ui_font_, kControlsButton, "Controles", mouse, pressed, true, 0, 24);
    if (controls_menu_.open) {
        controls_menu_.draw(window_, ui_font_, pause_gui_);
    }
}

void ClientApp::render() {
    window_.setActive(true);
    update_view();
    sync_world();
    const float elapsed_seconds = std::min(frame_clock_.restart().asSeconds(), 0.1f);
    update_adventure(elapsed_seconds);
    update_impacts(elapsed_seconds);
    automatic_background_x_ += elapsed_seconds * background::kAutomaticScrollSpeed;
    window_.clear(sf::Color::Black);
    draw_parallax();

    Animate(world_, elapsed_seconds);
    Draw(world_, window_);
    draw_area_attacks();
    show_deaths();
    explosions_.update(elapsed_seconds);
    explosions_.draw(window_);
    draw_hud();
    if (network_.is_spectator()) {
        // Below the HUD row, which may overlay the top of the play area.
        const sf::FloatRect banner{340.f, 66.f, 600.f, 56.f};
        pause_gui_.surface(window_, banner);
        pause_gui_.centered(window_, ui_font_, "Spectateur - tu joueras au prochain niveau", banner, 22);
    }
    if (results_open_) {
        draw_results();
    } else {
        draw_pause_menu();
    }
    window_.display();
}

} // namespace ntype::client
