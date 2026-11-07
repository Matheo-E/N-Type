#include "Launcher.hpp"
#include "GuiSkin.hpp"
#include "ControlsMenu.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <cctype>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <string>
#include <vector>

#include <SFML/Audio/Music.hpp>
#include <SFML/Graphics.hpp>

#include "BackgroundConfig.hpp"
#include "Protocol.hpp"
#include "UiClickSound.hpp"

namespace ntype::client {
namespace {

struct Field {
    std::string label;
    std::string value;
    sf::FloatRect bounds;
};

std::optional<sf::Font> load_font() {
    const std::array<const char*, 5> paths{
#ifdef _WIN32
        "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
#endif
        "/usr/local/share/fonts/DejaVuSans.ttf", "DejaVuSans.ttf", "Arial.ttf"};
    for (const auto* path : paths) {
        sf::Font font;
        if (font.loadFromFile(path)) return font;
    }
    return std::nullopt;
}

void draw_text(sf::RenderWindow& window, const sf::Font& font, const std::string& value,
               const float x, const float y, const unsigned size = 20,
               const sf::Color color = sf::Color::White) {
    auto text = make_ui_text(window, font, value, size);
    text.setFillColor(color);
    position_ui_text(window, text, x, y);
    window.draw(text);
}

/**
 * @brief Fills an area with a texture while preserving its aspect ratio.
 *
 * The excess part of the image is cropped symmetrically. This avoids stretching
 * launcher background when the window is resized or displayed fullscreen.
 *
 * @param window Render target receiving the background.
 * @param texture Background texture to draw.
 * @param destination Area that must be completely covered.
 */
void draw_texture_cover(sf::RenderWindow& window, const sf::Texture& texture,
                        const sf::FloatRect& destination) {
    const auto texture_size = texture.getSize();
    const float texture_ratio =
        static_cast<float>(texture_size.x) / static_cast<float>(texture_size.y);
    const float destination_ratio = destination.width / destination.height;
    sf::IntRect source(0, 0, static_cast<int>(texture_size.x),
                       static_cast<int>(texture_size.y));

    if (texture_ratio > destination_ratio) {
        source.width = static_cast<int>(static_cast<float>(texture_size.y) * destination_ratio);
        source.left = (static_cast<int>(texture_size.x) - source.width) / 2;
    } else if (texture_ratio < destination_ratio) {
        source.height = static_cast<int>(static_cast<float>(texture_size.x) / destination_ratio);
        source.top = (static_cast<int>(texture_size.y) - source.height) / 2;
    }
    draw_texture_region(window, texture, source, destination);
}

void draw_logo(sf::RenderWindow& window, const sf::Texture& texture,
               const sf::FloatRect& bounds) {
    const sf::Vector2u size = texture.getSize();
    const float scale = std::min(bounds.width / static_cast<float>(size.x),
                                 bounds.height / static_cast<float>(size.y));
    sf::Sprite logo(texture);
    logo.setScale(scale, scale);
    logo.setPosition(bounds.left + (bounds.width - static_cast<float>(size.x) * scale) / 2.f,
                     bounds.top + (bounds.height - static_cast<float>(size.y) * scale) / 2.f);
    window.draw(logo);
}

void update_launcher_view(sf::RenderWindow& window) {
    constexpr float logical_height = 410.f;
    const auto size = window.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float logical_width = logical_height * static_cast<float>(size.x) /
                                static_cast<float>(size.y);
    sf::View view(sf::FloatRect((560.f - logical_width) / 2.f, 0.f, logical_width,
                               logical_height));
    window.setView(view);
}

sf::FloatRect current_view_bounds(const sf::RenderWindow& window) {
    const auto& view = window.getView();
    const auto size = view.getSize();
    const auto center = view.getCenter();
    return {center.x - size.x / 2.f, center.y - size.y / 2.f, size.x, size.y};
}

std::string truncated(const std::string& text, const std::size_t limit) {
    return text.size() > limit ? text.substr(0, limit - 3) + "..." : text;
}

// Same order as the player ids 1 to 4: blue, orange, green, pink.
const std::array<sf::Color, 4> kPlayerColors{
    sf::Color(66, 135, 245), sf::Color(245, 150, 40), sf::Color(80, 190, 90), sf::Color(240, 110, 180)};

} // namespace

bool show_launcher(sf::RenderWindow& window, WindowState& window_state, ConnectionSettings& settings,
                   std::unique_ptr<ClientNetwork>& session, const LauncherStart start) {
    auto font = load_font();
    if (!font) throw std::runtime_error("Unable to load the UI font");
    auto& click_sound = UiClickSound::instance();
    click_sound.set_enabled(settings.click_sound_enabled);
    const std::string assets = NTYPE_ASSET_DIR;
    sf::Texture lobby_background, title_background, game_logo;
    GuiSkin gui;
    const auto lobby_path = assets + "/backgrounds/lobby/back_lobby.png";
    if (!lobby_background.loadFromFile(std::filesystem::exists(lobby_path)
            ? lobby_path : assets + "/backgrounds/lobby/back_menu.png") ||
        !title_background.loadFromFile(assets + "/backgrounds/lobby/back_play.png") ||
        !game_logo.loadFromFile(assets + "/sprites/La Contre-Attaque de M. Jean-cive.png") ||
        !gui.buttons.loadFromFile(assets + "/FreeGUI/png/Button.png") ||
        !gui.windows.loadFromFile(assets + "/FreeGUI/png/Window.png"))
        throw std::runtime_error("Unable to load the launcher assets");
    for (auto* texture : {&lobby_background, &title_background, &game_logo,
                          &gui.buttons, &gui.windows})
        texture->setSmooth(true);
    sf::Music music;
    if (music.openFromFile(assets + "/audio/music/lobby.mp3")) {
        music.setLoop(true);
        music.setVolume(40.f);
        if (settings.music_enabled) music.play();
    }
    window_state.open(window, "N-Type");
    update_launcher_view(window);

    enum class Page { Title, Connection, Home, Lobbies, Create, Waiting };
    Page page = Page::Title;
    if (session && start != LauncherStart::Title)
        page = session->phase() == Phase::InLobby ? Page::Waiting : Page::Lobbies;
    std::string selected_code;
    std::size_t first_visible_lobby = 0;
    constexpr std::size_t visible_lobbies = 3;
    std::array<Field, 3> fields{{
        {"Nom", settings.name, {210.f, 110.f, 265.f, 46.f}},
        {"Host", settings.host, {210.f, 172.f, 265.f, 46.f}},
        {"Port", std::to_string(settings.port), {210.f, 234.f, 265.f, 46.f}},
    }};
    Field lobby_field{"Nom du lobby", "", {200.f, 119.f, 275.f, 46.f}};
    Field code_field{"Code", "", {80.f, 307.f, 92.f, 52.f}};
    Field rename_field{"Nom du lobby", "", {110.f, 98.f, 340.f, 38.f}};
    bool code_focused = false;
    bool renaming = false;
    bool solo_pending = false; // "Play solo": start the lobby as soon as it exists.
    std::size_t active = 0;
    bool settings_open = false;
    ControlsMenu controls_menu;
    std::string error;
    const sf::FloatRect panel{55.f, 60.f, 450.f, 315.f};
    const sf::FloatRect play_button{200.f, 330.f, 160.f, 70.f};
    const sf::FloatRect connect_button{175.f, 306.f, 210.f, 55.f};
    const sf::FloatRect join_button{85.f, 118.f, 215.f, 58.f};
    const sf::FloatRect create_button{85.f, 198.f, 215.f, 58.f};
    const sf::FloatRect solo_button{85.f, 278.f, 215.f, 58.f};
    const sf::FloatRect join_code{177.f, 307.f, 58.f, 52.f};
    const sf::FloatRect join_selected{250.f, 307.f, 225.f, 52.f};
    const sf::FloatRect make_lobby{175.f, 294.f, 210.f, 57.f};
    const sf::FloatRect start_game{155.f, 292.f, 250.f, 58.f};
    const sf::FloatRect lobby_title{80.f, 98.f, 400.f, 38.f};
    const sf::FloatRect back_button{15.f, 12.f, 43.f, 43.f};
    const sf::FloatRect settings_button{502.f, 12.f, 43.f, 43.f};
    const sf::FloatRect music_toggle{170.f, 151.f, 220.f, 55.f};
    const sf::FloatRect click_toggle{170.f, 220.f, 220.f, 55.f};
    const sf::FloatRect controls_button{170.f, 287.f, 220.f, 55.f};

    const auto busy = [&]() { return session && session->busy(); };
    const auto lobby_row = [](const std::size_t row) {
        return sf::FloatRect{90.f, 119.f + static_cast<float>(row) * 56.f, 380.f, 49.f};
    };
    const auto go_back = [&]() {
        error.clear();
        if (session) session->clear_error();
        solo_pending = false;
        renaming = false;
        code_focused = false;
        if (page == Page::Waiting) {
            session->leave();
            page = Page::Lobbies;
        } else if (page == Page::Lobbies || page == Page::Create) {
            page = Page::Home;
        } else if (page == Page::Home) {
            session.reset(); // Disconnects from the server.
            page = Page::Connection;
        } else if (page == Page::Connection) {
            session.reset();
            page = Page::Title;
        }
    };
    const auto connect = [&]() {
        unsigned parsed = 0;
        const auto& text = fields[2].value;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
        if (fields[0].value.find_first_not_of(' ') == std::string::npos || fields[1].value.empty() ||
            result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
            parsed == 0 || parsed > 65535) {
            error = "Renseigne ton nom, le host et un port valide.";
            return;
        }
        if (!is_valid_name(fields[0].value)) {
            error = "Le pseudo fait 1 a 16 caracteres, sans accents.";
            return;
        }
        settings.name = fields[0].value;
        settings.host = fields[1].value;
        settings.port = static_cast<std::uint16_t>(parsed);
        error.clear();
        session.reset(); // Drops a previous attempt.
        try {
            session = std::make_unique<ClientNetwork>(settings.host, settings.port, settings.name);
        } catch (const std::exception&) {
            error = "Host introuvable : " + settings.host;
        }
        // The page changes once the server accepts us (see the session follow-up below).
    };
    const auto create_lobby = [&](const std::string& name) {
        if (!session || busy() || name.find_first_not_of(' ') == std::string::npos) return;
        error.clear();
        session->create(name);
    };
    const auto join_by_code = [&]() {
        if (!session || busy() || code_field.value.size() != protocol::kCodeLength) return;
        error.clear();
        session->join(code_field.value);
    };
    const auto rename_lobby = [&]() {
        renaming = false;
        const auto& lobby = session->lobby();
        if (lobby && !rename_field.value.empty() && rename_field.value != lobby->name)
            session->rename(rename_field.value);
    };
    const auto draw_field = [&](const Field& field, bool focused) {
        gui.surface(window, field.bounds, true);
        if (focused) {
            sf::RectangleShape outline({field.bounds.width - 4.f, field.bounds.height - 4.f});
            outline.setPosition(field.bounds.left + 2.f, field.bounds.top + 2.f);
            outline.setFillColor(sf::Color::Transparent);
            outline.setOutlineColor(sf::Color(36, 172, 210));
            outline.setOutlineThickness(1.5f);
            window.draw(outline);
        }
        const auto placeholder = field.label == "Nom" ? "Ton pseudo" : field.label;
        auto value = make_ui_text(window, *font, field.value.empty() ? placeholder : field.value, 17);
        // Keep long editable values within their field by showing the end.
        if (!field.value.empty()) {
            std::string visible = field.value;
            while (value.getGlobalBounds().width > field.bounds.width - 30.f && !visible.empty()) {
                visible.erase(visible.begin());
                value.setString(visible);
            }
        }
        value.setFillColor(field.value.empty() ? style::muted : style::ink);
        position_ui_text(window, value, field.bounds.left + 13.f,
                         field.bounds.top + (field.bounds.height - 24.f) / 2.f);
        window.draw(value);
    };

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (controls_menu.open && controls_menu.handle_event(window, event)) {
                continue;
            }
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                if (settings_open) settings_open = false;
                else if (renaming) renaming = false;
                else if (page == Page::Title) window.close();
                else go_back();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
                window_state.toggle_fullscreen(window, "N-Type");
                update_launcher_view(window);
            }
            if (event.type == sf::Event::Resized) update_launcher_view(window);
            if (event.type == sf::Event::MouseWheelScrolled && page == Page::Lobbies && !settings_open) {
                const std::size_t count = session ? session->lobbies().size() : 0;
                if (event.mouseWheelScroll.delta < 0 && first_visible_lobby + visible_lobbies < count)
                    ++first_visible_lobby;
                else if (event.mouseWheelScroll.delta > 0 && first_visible_lobby > 0)
                    --first_visible_lobby;
            }
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                const auto point = window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                if (page == Page::Title) {
                    if (play_button.contains(point)) {
                        click_sound.play();
                        page = Page::Connection;
                    }
                    continue;
                }
                if (settings_button.contains(point)) {
                    click_sound.play();
                    settings_open = !settings_open;
                    continue;
                }
                if (settings_open) {
                    if (controls_button.contains(point)) {
                        click_sound.play();
                        controls_menu.open = true;
                    } else if (music_toggle.contains(point)) {
                        click_sound.play();
                        settings.music_enabled = !settings.music_enabled;
                        if (settings.music_enabled) music.play(); else music.pause();
                    } else if (click_toggle.contains(point)) {
                        click_sound.play();
                        click_sound.set_enabled(!click_sound.enabled());
                    }
                    continue;
                }
                if (back_button.contains(point)) {
                    click_sound.play();
                    go_back();
                    continue;
                }
                if (page == Page::Connection) {
                    for (std::size_t i = 0; i < fields.size(); ++i)
                        if (fields[i].bounds.contains(point)) active = i;
                    if (connect_button.contains(point)) {
                        click_sound.play();
                        connect();
                    }
                } else if (page == Page::Home && !busy()) {
                    if (join_button.contains(point)) {
                        click_sound.play();
                        session->clear_error();
                        page = Page::Lobbies;
                    } else if (create_button.contains(point)) {
                        click_sound.play();
                        session->clear_error();
                        lobby_field.value = truncated(settings.name + "'s lobby", protocol::kMaxLobbyName);
                        page = Page::Create;
                    } else if (solo_button.contains(point)) {
                        click_sound.play();
                        solo_pending = true;
                        create_lobby(truncated(settings.name + " (solo)", protocol::kMaxLobbyName));
                    }
                } else if (page == Page::Lobbies) {
                    const auto& lobbies = session->lobbies();
                    for (std::size_t row = 0; row < visible_lobbies && first_visible_lobby + row < lobbies.size(); ++row) {
                        if (lobby_row(row).contains(point)) {
                            click_sound.play();
                            selected_code = lobbies[first_visible_lobby + row].code;
                        }
                    }
                    code_focused = code_field.bounds.contains(point);
                    if (join_code.contains(point) && code_field.value.size() == protocol::kCodeLength) {
                        click_sound.play();
                        join_by_code();
                    }
                    if (!selected_code.empty() && join_selected.contains(point) && !busy()) {
                        click_sound.play();
                        error.clear();
                        session->join(selected_code);
                    }
                } else if (page == Page::Create) {
                    if (make_lobby.contains(point)) {
                        click_sound.play();
                        create_lobby(lobby_field.value);
                    }
                } else if (page == Page::Waiting && session->lobby()) {
                    const auto& lobby = *session->lobby();
                    if (session->is_leader() && lobby_title.contains(point)) {
                        if (!renaming) rename_field.value = lobby.name;
                        renaming = true;
                    } else {
                        renaming = false;
                    }
                    if (session->is_leader() && start_game.contains(point) && !busy()) {
                        click_sound.play();
                        session->start();
                    }
                }
            }
            if (settings_open) continue;
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Tab && page == Page::Connection)
                    active = (active + 1) % fields.size();
                if (event.key.code == sf::Keyboard::Return) {
                    if (page == Page::Title) { click_sound.play(); page = Page::Connection; }
                    else if (page == Page::Connection) { click_sound.play(); connect(); }
                    else if (page == Page::Create) { click_sound.play(); create_lobby(lobby_field.value); }
                    else if (page == Page::Lobbies && code_focused) join_by_code();
                    else if (page == Page::Waiting && renaming) rename_lobby();
                }
            }
            if (event.type == sf::Event::TextEntered) {
                std::string* value = nullptr;
                std::size_t limit = 40;
                if (page == Page::Connection) {
                    value = &fields[active].value;
                    if (active == 0) limit = protocol::kMaxPlayerName;
                }
                if (page == Page::Create) {
                    value = &lobby_field.value;
                    limit = protocol::kMaxLobbyName;
                }
                if (page == Page::Lobbies && code_focused) {
                    value = &code_field.value;
                    limit = protocol::kCodeLength;
                }
                if (page == Page::Waiting && renaming) {
                    value = &rename_field.value;
                    limit = protocol::kMaxLobbyName;
                }
                if (value) {
                    const auto c = event.text.unicode;
                    if (c == 8 && !value->empty()) value->pop_back();
                    else if (c >= 32 && c < 127 && value->size() < limit) {
                        char typed = static_cast<char>(c);
                        // Room codes only use letters and digits, shown in upper case.
                        if (value == &code_field.value) {
                            typed = static_cast<char>(std::toupper(static_cast<unsigned char>(typed)));
                            if (protocol::kCodeAlphabet.find(typed) == std::string_view::npos) continue;
                        }
                        value->push_back(typed);
                    }
                }
            }
        }
        if (!window.isOpen()) break;

        // Follow the session: it decides when we are connected, in a lobby or in game.
        if (session) {
            session->update(0); // Also the heartbeat that keeps us connected.
            const Phase phase = session->phase();
            if (page == Page::Connection && phase == Phase::Browsing) {
                page = Page::Home;
                error.clear();
            }
            if (phase == Phase::InLobby) {
                if (page != Page::Waiting) {
                    page = Page::Waiting;
                    renaming = false;
                }
                if (session->lobby()->state == protocol::LobbyPhase::Playing) {
                    settings.click_sound_enabled = click_sound.enabled();
                    return true;
                }
                if (solo_pending && session->is_leader() && !busy()) {
                    solo_pending = false;
                    session->start();
                }
            } else if (page == Page::Waiting) {
                page = Page::Lobbies; // Our leave went through, or the connection was lost.
            }
            if (!session->error().empty()) solo_pending = false;
            const auto& lobbies = session->lobbies();
            if (first_visible_lobby + visible_lobbies > lobbies.size())
                first_visible_lobby = lobbies.size() > visible_lobbies ? lobbies.size() - visible_lobbies : 0;
            if (std::none_of(lobbies.begin(), lobbies.end(),
                             [&](const auto& lobby) { return lobby.code == selected_code; }))
                selected_code.clear();
        }

        const auto view = current_view_bounds(window);
        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        window.clear();
        draw_texture_cover(window, page == Page::Title ? title_background : lobby_background, view);
        if (page == Page::Title) {
            const float logo_width = std::min(400.f, view.width - 32.f);
            draw_logo(window, game_logo, {(560.f - logo_width) / 2.f, 16.f, logo_width, 134.f});
            gui.button(window, *font, play_button, "PLAY", mouse, pressed, true, 2, 29);
            window.display();
            continue;
        }
        if (page == Page::Connection) {
            gui.panel(window, *font, "Connexion", panel);
            for (std::size_t i = 0; i < fields.size(); ++i) {
                draw_text(window, *font, fields[i].label, 88.f, fields[i].bounds.top + 12.f, 18, style::ink);
                draw_field(fields[i], i == active && !settings_open);
            }
            // Pressing again retries, with the fields as they are now.
            gui.button(window, *font, connect_button, session ? "Reessayer" : "Se connecter",
                       mouse, pressed, true, 1);
        } else if (page == Page::Home) {
            gui.surface(window, panel);
            draw_logo(window, game_logo, {170.f, 32.f, 220.f, 74.f});
            gui.button(window, *font, join_button, "Join lobby", mouse, pressed, !busy());
            gui.button(window, *font, create_button, "Create lobby", mouse, pressed, !busy(), 1);
            gui.button(window, *font, solo_button, "Play solo", mouse, pressed, !busy(), 2);
            gui.surface(window, {327.f, 119.f, 149.f, 216.f}, true);
            gui.icon(window, {200, 2060, 185, 195}, {377.f, 135.f, 49.f, 51.f});
            gui.centered(window, *font, "MEILLEUR", {330.f, 202.f, 143.f, 24.f}, 15);
            gui.centered(window, *font, "SCORE", {330.f, 225.f, 143.f, 24.f}, 18);
            gui.centered(window, *font, "--", {330.f, 254.f, 143.f, 36.f}, 30);
            gui.centered(window, *font, "Bientot disponible", {330.f, 300.f, 143.f, 18.f}, 10, style::muted);
        } else if (page == Page::Lobbies) {
            const auto& lobbies = session->lobbies();
            gui.panel(window, *font, "Join lobby", panel);
            gui.centered(window, *font, "Choisis un lobby ou entre son code", {80.f, 98.f, 400.f, 18.f}, 12,
                         style::muted);
            if (lobbies.empty()) {
                gui.centered(window, *font, "Aucun lobby disponible", {80.f, 158.f, 400.f, 40.f}, 20);
                gui.centered(window, *font, "Cree un lobby depuis le menu.", {80.f, 207.f, 400.f, 30.f}, 14, style::muted);
            }
            for (std::size_t row = 0; row < visible_lobbies && first_visible_lobby + row < lobbies.size(); ++row) {
                const auto& lobby = lobbies[first_visible_lobby + row];
                const auto bounds = lobby_row(row);
                gui.surface(window, bounds, true);
                if (lobby.code == selected_code)
                    gui.icon(window, {3090, 755, 185, 195}, {99.f, bounds.top + 7.f, 32.f, 33.f});
                draw_text(window, *font, truncated(lobby.name, 20), 141.f, bounds.top + 7.f, 16, style::ink);
                draw_text(window, *font, lobby.code + (lobby.state == protocol::LobbyPhase::Playing
                                                           ? "  -  en jeu" : "  -  en attente"),
                          141.f, bounds.top + 28.f, 11, style::muted);
                draw_text(window, *font, std::to_string(lobby.members) + " / 4", 414.f, bounds.top + 14.f, 14,
                          style::muted);
            }
            draw_field(code_field, code_focused && !settings_open);
            gui.button(window, *font, join_code, "OK", mouse, pressed,
                       code_field.value.size() == protocol::kCodeLength && !busy(), 1, 16);
            gui.button(window, *font, join_selected, "Rejoindre", mouse, pressed,
                       !selected_code.empty() && !busy(), 1);
            if (lobbies.size() > visible_lobbies)
                draw_text(window, *font, "Molette pour parcourir les lobbies", 167.f, 360.f, 11, style::muted);
        } else if (page == Page::Create) {
            gui.panel(window, *font, "Create lobby", panel);
            draw_text(window, *font, "Nom du lobby", 83.f, 132.f, 15, style::ink);
            draw_field(lobby_field, !settings_open);
            gui.centered(window, *font, "Jusqu'a 4 joueurs par lobby", {80.f, 207.f, 400.f, 47.f}, 16, style::muted);
            gui.button(window, *font, make_lobby, "Creer le lobby", mouse, pressed,
                       lobby_field.value.find_first_not_of(' ') != std::string::npos && !busy(), 1);
        } else if (page == Page::Waiting && session->lobby()) {
            const auto& lobby = *session->lobby();
            const bool leader = session->is_leader();
            gui.panel(window, *font, "Salle d'attente", panel);
            if (renaming) draw_field(rename_field, true);
            else gui.centered(window, *font, lobby.name, lobby_title, 20);
            gui.centered(window, *font, "Code : " + lobby.code, {80.f, 136.f, 400.f, 22.f}, 15, style::muted);
            for (std::size_t i = 0; i < lobby.members.size(); ++i) {
                const auto& member = lobby.members[i];
                const sf::FloatRect row{100.f, 162.f + static_cast<float>(i) * 31.f, 360.f, 28.f};
                gui.surface(window, row, true);
                sf::RectangleShape colour({14.f, 14.f});
                colour.setPosition(row.left + 10.f, row.top + 7.f);
                colour.setFillColor(kPlayerColors[(member.slot - 1U) % kPlayerColors.size()]);
                window.draw(colour);
                std::string name = member.name;
                if (member.slot == lobby.yourSlot) name += " (toi)";
                draw_text(window, *font, name, row.left + 32.f, row.top + 5.f, 14, style::ink);
                std::string tags;
                if (member.slot == lobby.leaderSlot) tags = "chef";
                if (member.role == protocol::MemberRole::Spectator)
                    tags += tags.empty() ? "spectateur" : ", spectateur";
                draw_text(window, *font, tags, row.left + 262.f, row.top + 7.f, 11, style::muted);
            }
            if (leader) {
                gui.button(window, *font, start_game, "Lancer la partie", mouse, pressed, !busy(), 1, 17);
                if (!renaming)
                    gui.centered(window, *font, "Clique sur le nom pour le changer", {80.f, 352.f, 400.f, 16.f},
                                 10, style::muted);
            } else {
                gui.button(window, *font, start_game, "En attente du chef...", mouse, pressed, false, 1, 16);
            }
        }
        gui.icon(window, {200, 2710, 185, 195}, back_button);
        gui.icon(window, {3090, 315, 185, 195}, settings_button);
        // Our own message first, then the server's refusal, then the connection state.
        std::string message = error;
        if (message.empty() && session) message = session->error();
        if (message.empty() && session && session->phase() == Phase::Connecting) message = session->status();
        if (!message.empty()) {
            gui.surface(window, {35.f, 377.f, 490.f, 29.f});
            gui.centered(window, *font, message, {40.f, 377.f, 480.f, 26.f}, 11, sf::Color(157, 36, 49));
        }
        if (settings_open) {
            sf::RectangleShape shade({view.width, view.height});
            shade.setPosition(view.left, view.top);
            shade.setFillColor(sf::Color(21, 37, 67, 160));
            window.draw(shade);
            gui.panel(window, *font, "Reglages", {130.f, 104.f, 300.f, 280.f});
            gui.button(window, *font, music_toggle, settings.music_enabled ? "Musique : ON" : "Musique : OFF",
                       mouse, pressed, true, settings.music_enabled ? 1 : 2);
            gui.button(window, *font, click_toggle, click_sound.enabled() ? "Son : ON" : "Son : OFF",
                       mouse, pressed, true, click_sound.enabled() ? 1 : 2);
            gui.button(window, *font, controls_button, "Controles", mouse, pressed);
            gui.centered(window, *font, "F11 : plein ecran", {150.f, 350.f, 260.f, 23.f}, 13, style::muted);
            gui.icon(window, {3090, 315, 185, 195}, settings_button);
        }
        if (controls_menu.open) {
            controls_menu.draw(window, *font, gui);
        }
        window.display();
    }
    settings.click_sound_enabled = click_sound.enabled();
    return false;
}

} // namespace ntype::client
