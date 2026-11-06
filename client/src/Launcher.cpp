#include "Launcher.hpp"
#include "GuiSkin.hpp"

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
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

bool server_is_reachable(const std::string& host, const std::uint16_t port, std::string& error) {
    using namespace std::chrono_literals;
    asio::io_context io;
    std::error_code network_error;
    asio::ip::udp::resolver resolver(io);
    const auto endpoints = resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port), network_error);
    if (network_error || endpoints.empty()) {
        error = "Host not found. Check the host name or IP address.";
        return false;
    }
    const auto server = endpoints.begin()->endpoint();
    asio::ip::udp::socket socket(io);
    socket.open(asio::ip::udp::v4(), network_error);
    if (!network_error) socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), network_error);
    if (!network_error) socket.non_blocking(true, network_error);
    if (network_error) {
        error = "Unable to open the network connection.";
        return false;
    }

    const auto request = protocol::makeConnectRequest();
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    auto next_send = std::chrono::steady_clock::time_point{};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_send) {
            socket.send_to(asio::buffer(request), server, 0, network_error);
            // A closed UDP port may report connection_refused. It is an expected
            // validation result, not an exception that should close the UI.
            if (network_error && network_error != asio::error::connection_refused) break;
            network_error.clear();
            next_send = now + 400ms;
        }
        std::array<std::uint8_t, 1500> bytes{};
        asio::ip::udp::endpoint from;
        const auto size = socket.receive_from(asio::buffer(bytes), from, 0, network_error);
        if (!network_error && from == server) {
            const auto packet = protocol::decode(
                {bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(size)});
            if (packet && packet->type == protocol::MessageType::ConnectAccept) {
                const auto disconnect = protocol::makeDisconnect();
                socket.send_to(asio::buffer(disconnect), server, 0, network_error);
                error.clear();
                return true;
            }
            if (packet && packet->type == protocol::MessageType::ConnectRefuse) {
                error = "The server is full. Please choose another lobby.";
                return false;
            }
        }
        if (network_error != asio::error::would_block && network_error != asio::error::try_again &&
            network_error != asio::error::connection_refused) {
            break;
        }
        network_error.clear();
        std::this_thread::sleep_for(20ms);
    }
    error = "No server answered on " + host + ":" + std::to_string(port) + ".";
    return false;
}

} // namespace

std::optional<ConnectionSettings> show_launcher(sf::RenderWindow& window, WindowState& window_state,
                                                const ConnectionSettings& defaults,
                                                const bool open_browser) {
    auto font = load_font();
    if (!font) return std::nullopt;
    auto& click_sound = UiClickSound::instance();
    click_sound.set_enabled(defaults.click_sound_enabled);
    bool music_enabled = defaults.music_enabled;
    const std::string assets = NTYPE_ASSET_DIR;
    sf::Texture lobby_background, title_background;
    GuiSkin gui;
    const auto lobby_path = assets + "/backgrounds/lobby/back_lobby.png";
    if (!lobby_background.loadFromFile(std::filesystem::exists(lobby_path)
            ? lobby_path : assets + "/backgrounds/lobby/back_menu.png") ||
        !title_background.loadFromFile(assets + "/backgrounds/lobby/back_play.png") ||
        !gui.buttons.loadFromFile(assets + "/FreeGUI/png/Button.png") ||
        !gui.windows.loadFromFile(assets + "/FreeGUI/png/Window.png"))
        return std::nullopt;
    for (auto* texture : {&lobby_background, &title_background, &gui.buttons, &gui.windows})
        texture->setSmooth(true);
    sf::Music music;
    if (music.openFromFile(assets + "/audio/music/lobby.mp3")) {
        music.setLoop(true);
        music.setVolume(40.f);
        if (music_enabled) music.play();
    }
    window_state.open(window, "N-Type");
    update_launcher_view(window);

    enum class Page { Title, Connection, Home, Lobbies, Create, Waiting };
    Page page = open_browser ? Page::Home : Page::Title;
    struct Lobby { std::string name; unsigned size; };
    std::vector<Lobby> lobbies;
    std::size_t selected_lobby = 0;
    std::size_t first_visible_lobby = 0;
    constexpr std::size_t visible_lobbies = 3;
    std::array<Field, 3> fields{{
        {"Nom", defaults.name, {210.f, 110.f, 265.f, 46.f}},
        {"Host", defaults.host, {210.f, 172.f, 265.f, 46.f}},
        {"Port", std::to_string(defaults.port), {210.f, 234.f, 265.f, 46.f}},
    }};
    Field lobby_field{"Nom du lobby", "My lobby", {200.f, 119.f, 275.f, 46.f}};
    std::string player_name = defaults.name;
    std::string host = defaults.host;
    std::uint16_t port = defaults.port;
    std::string lobby_name;
    unsigned lobby_size = 4;
    unsigned current_players = 1;
    std::size_t active = 0;
    bool settings_open = false;
    std::string error;
    const sf::FloatRect panel{55.f, 60.f, 450.f, 315.f};
    const sf::FloatRect play_button{200.f, 330.f, 160.f, 70.f};
    const sf::FloatRect connect_button{175.f, 306.f, 210.f, 55.f};
    const sf::FloatRect join_button{85.f, 118.f, 215.f, 58.f};
    const sf::FloatRect create_button{85.f, 198.f, 215.f, 58.f};
    const sf::FloatRect solo_button{85.f, 278.f, 215.f, 58.f};
    const sf::FloatRect join_selected{181.f, 307.f, 198.f, 52.f};
    const sf::FloatRect decrease{200.f, 207.f, 57.f, 47.f};
    const sf::FloatRect increase{418.f, 207.f, 57.f, 47.f};
    const sf::FloatRect make_lobby{175.f, 294.f, 210.f, 57.f};
    const sf::FloatRect enter_game{155.f, 292.f, 250.f, 58.f};
    const sf::FloatRect back_button{15.f, 12.f, 43.f, 43.f};
    const sf::FloatRect settings_button{502.f, 12.f, 43.f, 43.f};
    const sf::FloatRect music_toggle{170.f, 151.f, 220.f, 55.f};
    const sf::FloatRect click_toggle{170.f, 220.f, 220.f, 55.f};
    const auto go_back = [&]() {
        if (page == Page::Home) page = Page::Connection;
        else if (page == Page::Connection) page = Page::Title;
        else if (page == Page::Waiting) page = Page::Lobbies;
        else page = Page::Home;
        error.clear();
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
        player_name = fields[0].value;
        host = fields[1].value;
        port = static_cast<std::uint16_t>(parsed);
        page = Page::Home;
        error.clear();
    };
    const auto launch_game = [&]() -> std::optional<ConnectionSettings> {
        if (!server_is_reachable(host, port, error)) return std::nullopt;
        return ConnectionSettings{player_name, host, port, music_enabled, click_sound.enabled()};
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
        position_ui_text(window, value, field.bounds.left + 13.f, field.bounds.top + 11.f);
        window.draw(value);
    };

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                if (settings_open) settings_open = false;
                else if (page == Page::Title) window.close();
                else go_back();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
                window_state.toggle_fullscreen(window, "N-Type");
                update_launcher_view(window);
            }
            if (event.type == sf::Event::Resized) update_launcher_view(window);
            if (event.type == sf::Event::MouseWheelScrolled && page == Page::Lobbies && !settings_open) {
                if (event.mouseWheelScroll.delta < 0 && first_visible_lobby + visible_lobbies < lobbies.size())
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
                    if (music_toggle.contains(point)) {
                        click_sound.play();
                        music_enabled = !music_enabled;
                        if (music_enabled) music.play(); else music.pause();
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
                } else if (page == Page::Home) {
                    if (join_button.contains(point)) {
                        click_sound.play();
                        page = Page::Lobbies;
                    } else if (create_button.contains(point)) {
                        click_sound.play();
                        page = Page::Create;
                    } else if (solo_button.contains(point)) {
                        click_sound.play();
                        if (auto result = launch_game()) return result;
                    }
                } else if (page == Page::Lobbies) {
                    for (std::size_t row = 0; row < visible_lobbies && first_visible_lobby + row < lobbies.size(); ++row) {
                        const sf::FloatRect bounds{90.f, 119.f + static_cast<float>(row) * 56.f, 380.f, 49.f};
                        if (bounds.contains(point)) {
                            click_sound.play();
                            selected_lobby = first_visible_lobby + row;
                        }
                    }
                    if (!lobbies.empty() && join_selected.contains(point)) {
                        click_sound.play();
                        lobby_name = lobbies[selected_lobby].name;
                        lobby_size = lobbies[selected_lobby].size;
                        current_players = 1;
                        page = Page::Waiting;
                    }
                } else if (page == Page::Create) {
                    if (decrease.contains(point) && lobby_size > 2) {
                        click_sound.play();
                        --lobby_size;
                    }
                    if (increase.contains(point) && lobby_size < 4) {
                        click_sound.play();
                        ++lobby_size;
                    }
                    if (make_lobby.contains(point) && lobby_field.value.find_first_not_of(' ') != std::string::npos) {
                        click_sound.play();
                        lobbies.push_back({lobby_field.value, lobby_size});
                        selected_lobby = lobbies.size() - 1;
                        first_visible_lobby = lobbies.size() > visible_lobbies ? lobbies.size() - visible_lobbies : 0;
                        page = Page::Lobbies;
                        error.clear();
                    }
                } else if (page == Page::Waiting && enter_game.contains(point) && current_players == lobby_size) {
                    click_sound.play();
                    if (auto result = launch_game()) return result;
                }
            }
            if (settings_open) continue;
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Tab && page == Page::Connection)
                    active = (active + 1) % fields.size();
                if (event.key.code == sf::Keyboard::Return) {
                    if (page == Page::Title) { click_sound.play(); page = Page::Connection; }
                    else if (page == Page::Connection) { click_sound.play(); connect(); }
                }
            }
            if (event.type == sf::Event::TextEntered) {
                std::string* value = nullptr;
                if (page == Page::Connection) value = &fields[active].value;
                if (page == Page::Create) value = &lobby_field.value;
                if (value) {
                    if (event.text.unicode == 8 && !value->empty()) value->pop_back();
                    else if (event.text.unicode >= 32 && event.text.unicode < 127 && value->size() < 40)
                        value->push_back(static_cast<char>(event.text.unicode));
                }
            }
        }
        if (!window.isOpen()) break;
        const auto view = current_view_bounds(window);
        const auto mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const bool pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        window.clear();
        draw_texture_cover(window, page == Page::Title ? title_background : lobby_background, view);
        if (page == Page::Title) {
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
            gui.button(window, *font, connect_button, "Se connecter", mouse, pressed, true, 1);
        } else if (page == Page::Home) {
            gui.surface(window, panel);
            gui.button(window, *font, join_button, "Join lobby", mouse, pressed);
            gui.button(window, *font, create_button, "Create lobby", mouse, pressed, true, 1);
            gui.button(window, *font, solo_button, "Play solo", mouse, pressed, true, 2);
            gui.surface(window, {327.f, 119.f, 149.f, 216.f}, true);
            gui.icon(window, {200, 2060, 185, 195}, {377.f, 135.f, 49.f, 51.f});
            gui.centered(window, *font, "MEILLEUR", {330.f, 202.f, 143.f, 24.f}, 15);
            gui.centered(window, *font, "SCORE", {330.f, 225.f, 143.f, 24.f}, 18);
            gui.centered(window, *font, "--", {330.f, 254.f, 143.f, 36.f}, 30);
            gui.centered(window, *font, "Bientot disponible", {330.f, 300.f, 143.f, 18.f}, 10, style::muted);
        } else if (page == Page::Lobbies) {
            gui.panel(window, *font, "Join lobby", panel);
            draw_text(window, *font, "Choisis un lobby", 224.f, 101.f, 12, style::muted);
            if (lobbies.empty()) {
                gui.centered(window, *font, "Aucun lobby disponible", {80.f, 158.f, 400.f, 40.f}, 20);
                gui.centered(window, *font, "Cree un lobby depuis le menu.", {80.f, 207.f, 400.f, 30.f}, 14, style::muted);
            }
            for (std::size_t row = 0; row < visible_lobbies && first_visible_lobby + row < lobbies.size(); ++row) {
                const auto index = first_visible_lobby + row;
                const auto& lobby = lobbies[index];
                const sf::FloatRect bounds{90.f, 119.f + static_cast<float>(row) * 56.f, 380.f, 49.f};
                gui.surface(window, bounds, true);
                if (index == selected_lobby)
                    gui.icon(window, {3090, 755, 185, 195}, {99.f, bounds.top + 7.f, 32.f, 33.f});
                std::string name = lobby.name;
                if (name.size() > 23) name = name.substr(0, 20) + "...";
                draw_text(window, *font, name, 141.f, bounds.top + 13.f, 16, style::ink);
                draw_text(window, *font, "1 / " + std::to_string(lobby.size), 414.f, bounds.top + 14.f, 14, style::muted);
            }
            gui.button(window, *font, join_selected, "Rejoindre", mouse, pressed, !lobbies.empty(), 1);
            if (lobbies.size() > visible_lobbies)
                draw_text(window, *font, "Molette pour parcourir les lobbies", 167.f, 360.f, 11, style::muted);
        } else if (page == Page::Create) {
            gui.panel(window, *font, "Create lobby", panel);
            draw_text(window, *font, "Nom du lobby", 83.f, 132.f, 15, style::ink);
            draw_field(lobby_field, !settings_open);
            draw_text(window, *font, "Joueurs", 83.f, 219.f, 18, style::ink);
            gui.button(window, *font, decrease, "-", mouse, pressed, lobby_size > 2);
            gui.surface(window, {285.f, 207.f, 105.f, 47.f}, true);
            gui.centered(window, *font, std::to_string(lobby_size), {285.f, 207.f, 105.f, 47.f}, 24);
            gui.button(window, *font, increase, "+", mouse, pressed, lobby_size < 4);
            gui.button(window, *font, make_lobby, "Creer le lobby", mouse, pressed,
                       lobby_field.value.find_first_not_of(' ') != std::string::npos, 1);
        } else if (page == Page::Waiting) {
            gui.panel(window, *font, "Salle d'attente", panel);
            gui.centered(window, *font, lobby_name.substr(0, 27), {80.f, 112.f, 400.f, 43.f}, 22);
            gui.centered(window, *font, "Joueurs : " + std::to_string(current_players) + " / " + std::to_string(lobby_size),
                         {80.f, 178.f, 400.f, 37.f}, 20);
            const bool full = current_players == lobby_size;
            gui.button(window, *font, enter_game, full ? "Jouer" : "En attente de joueurs...",
                       mouse, pressed, full, 1, 16);
        }
        gui.icon(window, {200, 2710, 185, 195}, back_button);
        gui.icon(window, {3090, 315, 185, 195}, settings_button);
        if (!error.empty()) {
            gui.surface(window, {35.f, 377.f, 490.f, 29.f});
            gui.centered(window, *font, error, {40.f, 377.f, 480.f, 26.f}, 11, sf::Color(157, 36, 49));
        }
        if (settings_open) {
            sf::RectangleShape shade({view.width, view.height});
            shade.setPosition(view.left, view.top);
            shade.setFillColor(sf::Color(21, 37, 67, 160));
            window.draw(shade);
            gui.panel(window, *font, "Reglages", {130.f, 104.f, 300.f, 220.f});
            gui.button(window, *font, music_toggle, music_enabled ? "Musique : ON" : "Musique : OFF",
                       mouse, pressed, true, music_enabled ? 1 : 2);
            gui.button(window, *font, click_toggle, click_sound.enabled() ? "Son : ON" : "Son : OFF",
                       mouse, pressed, true, click_sound.enabled() ? 1 : 2);
            gui.centered(window, *font, "F11 : plein ecran", {150.f, 286.f, 260.f, 23.f}, 13, style::muted);
            gui.icon(window, {3090, 315, 185, 195}, settings_button);
        }
        window.display();
    }
    return std::nullopt;
}

} // namespace ntype::client
