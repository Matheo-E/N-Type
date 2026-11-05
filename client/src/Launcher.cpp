#include "Launcher.hpp"

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <thread>

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

namespace style {
const sf::Color panel{13, 39, 70, 230};
const sf::Color field{18, 49, 82, 245};
const sf::Color border{74, 130, 176};
const sf::Color primary{31, 111, 171};
const sf::Color primary_hover{42, 132, 198};
const sf::Color success{42, 132, 104};
const sf::Color success_hover{52, 151, 119};
const sf::Color secondary{76, 79, 146};
const sf::Color secondary_hover{91, 95, 167};
const sf::Color disabled{61, 72, 86};
const sf::Color muted_text{184, 202, 220};
} // namespace style

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
    sf::Text text(value, font, size);
    text.setFillColor(color);
    text.setPosition(x, y);
    window.draw(text);
}

/** @brief Draws a lightweight rounded UI surface without sprite assets. */
void draw_rounded_rect(sf::RenderWindow& window, const sf::FloatRect& bounds,
                       const float radius, const sf::Color fill,
                       const sf::Color outline = sf::Color::Transparent,
                       const float outline_thickness = 0.f) {
    constexpr int corner_steps = 5;
    constexpr float pi = 3.14159265358979323846f;
    sf::ConvexShape shape(static_cast<std::size_t>(corner_steps * 4));
    std::size_t index = 0;
    for (int corner = 0; corner < 4; ++corner) {
        const float center_x = bounds.left + (corner == 0 || corner == 3 ? radius
                                                                         : bounds.width - radius);
        const float center_y = bounds.top + (corner < 2 ? radius : bounds.height - radius);
        const float start = static_cast<float>(corner) * pi / 2.f + pi;
        for (int step = 0; step < corner_steps; ++step) {
            const float angle = start + static_cast<float>(step) * (pi / 2.f) /
                                            static_cast<float>(corner_steps - 1);
            shape.setPoint(index++, {center_x + std::cos(angle) * radius,
                                     center_y + std::sin(angle) * radius});
        }
    }
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(outline_thickness);
    window.draw(shape);
}

/** @brief Draws a centered text button with normal, hover, pressed and disabled states. */
void draw_button(sf::RenderWindow& window, const sf::Font& font, const sf::FloatRect& bounds,
                 const std::string& label, const sf::Vector2f& mouse, const bool pressed,
                 const bool enabled, const sf::Color normal, const sf::Color hover) {
    sf::Color fill = enabled ? normal : style::disabled;
    if (enabled && bounds.contains(mouse)) fill = pressed ? normal * sf::Color(210, 210, 210) : hover;
    draw_rounded_rect(window, bounds, 8.f, fill, style::border, 1.f);
    sf::Text text(label, font, 18);
    text.setFillColor(enabled ? sf::Color::White : sf::Color(160, 170, 180));
    const auto text_bounds = text.getLocalBounds();
    text.setPosition(bounds.left + (bounds.width - text_bounds.width) / 2.f - text_bounds.left,
                     bounds.top + (bounds.height - text_bounds.height) / 2.f - text_bounds.top - 1.f);
    window.draw(text);
}

/**
 * @brief Draws one region of a sprite sheet inside a destination rectangle.
 *
 * The source rectangle keeps the transparent pixels surrounding each login
 * element, which preserves the soft shadows from the login sprite sheet.
 *
 * @param window Render target receiving the sprite.
 * @param texture Sprite sheet containing the requested element.
 * @param source Pixel rectangle inside the sprite sheet.
 * @param destination Rectangle, in launcher coordinates, where it is drawn.
 */
void draw_texture_region(sf::RenderWindow& window, const sf::Texture& texture,
                         const sf::IntRect& source, const sf::FloatRect& destination) {
    sf::Sprite sprite(texture, source);
    sprite.setPosition(destination.left, destination.top);
    sprite.setScale(destination.width / static_cast<float>(source.width),
                    destination.height / static_cast<float>(source.height));
    window.draw(sprite);
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

std::optional<ConnectionSettings> show_launcher(sf::RenderWindow& window, bool& fullscreen,
                                                const ConnectionSettings& defaults,
                                                const bool open_browser) {
    auto font = load_font();
    if (!font) return std::nullopt;
    auto& click_sound = UiClickSound::instance();
    click_sound.set_enabled(defaults.click_sound_enabled);
    bool music_enabled = defaults.music_enabled;
    sf::Texture lobby_background;
    const std::string asset_directory = NTYPE_ASSET_DIR;
    if (!lobby_background.loadFromFile(asset_directory + "/sprites/lobby/bg_gradient.png"))
        return std::nullopt;
    lobby_background.setSmooth(true);
    sf::Music lobby_music;
    if (lobby_music.openFromFile(std::string(NTYPE_ASSET_DIR) + "/audio/music/lobby.mp3")) {
        lobby_music.setLoop(true);
        lobby_music.setVolume(40.f);
        if (music_enabled) lobby_music.play();
    }

    if (!window.isOpen()) {
        window.create(sf::VideoMode(700, 520), "N-Type - Connection", sf::Style::Default);
        fullscreen = false;
    } else {
        window.setTitle("N-Type - Connection");
        if (!fullscreen) window.setSize({700, 520});
    }
    window.setFramerateLimit(60);
    update_launcher_view(window);
    std::array<Field, 3> connection_fields{{
        {"Name", defaults.name, {194.f, 112.f, 346.f, 67.f}},
        {"Host", defaults.host, {194.f, 181.f, 346.f, 67.f}},
        {"Port", std::to_string(defaults.port), {194.f, 250.f, 346.f, 67.f}},
    }};
    std::array<Field, 1> lobby_fields{{{"Lobby name", "My lobby", {170.f, 105.f, 330.f, 48.f}}}};
    enum class Page { Connection, Browser, Create, Waiting };
    Page page = open_browser ? Page::Browser : Page::Connection;
    std::string player_name = defaults.name;
    std::string host = defaults.host;
    std::uint16_t selected_port = defaults.port;
    std::string lobby_name = "Local lobby";
    unsigned lobby_size = 4;
    unsigned current_players = 1;
    bool lobby_created = false;
    bool settings_open = false;
    std::size_t active = 0;
    std::string error;
    const sf::FloatRect button{150.f, 320.f, 350.f, 52.f};
    const sf::FloatRect connection_button{194.f, 328.f, 346.f, 46.f};
    const sf::FloatRect join_button{45.f, 250.f, 205.f, 52.f};
    const sf::FloatRect create_button{310.f, 150.f, 205.f, 46.f};
    const sf::FloatRect solo_button{310.f, 230.f, 205.f, 48.f};
    const sf::FloatRect decrease_players_button{170.f, 205.f, 70.f, 40.f};
    const sf::FloatRect increase_players_button{430.f, 205.f, 70.f, 40.f};
    const sf::FloatRect make_lobby_button{170.f, 280.f, 330.f, 48.f};
    const sf::FloatRect back_button{18.f, 12.f, 46.f, 32.f};
    const sf::FloatRect settings_button{500.f, 12.f, 44.f, 34.f};
    const sf::FloatRect music_toggle{170.f, 145.f, 220.f, 48.f};
    const sf::FloatRect click_toggle{170.f, 210.f, 220.f, 48.f};

    const auto connect = [&]() -> bool {
        unsigned parsed = 0;
        const auto& port = connection_fields[2].value;
        const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed);
        if (connection_fields[0].value.empty() || connection_fields[1].value.empty() ||
            result.ec != std::errc{} || result.ptr != port.data() + port.size() ||
            parsed == 0 || parsed > 65535) {
            error = "Please enter a name, host and a valid port.";
            return false;
        }
        player_name = connection_fields[0].value;
        host = connection_fields[1].value;
        selected_port = static_cast<std::uint16_t>(parsed);
        error.clear();
        page = Page::Browser;
        return true;
    };

    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed ||
                (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape))
                window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
                fullscreen = !fullscreen;
                if (fullscreen) {
                    window.create(sf::VideoMode::getDesktopMode(), "N-Type - Connection",
                                  sf::Style::Fullscreen);
                } else {
                    window.create(sf::VideoMode(700, 520), "N-Type - Connection",
                                  sf::Style::Default);
                }
                window.setFramerateLimit(60);
                update_launcher_view(window);
            }
            if (event.type == sf::Event::Resized) update_launcher_view(window);
            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left) {
                const sf::Vector2f point =
                    window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                if (settings_button.contains(point)) {
                    click_sound.play();
                    settings_open = !settings_open;
                    continue;
                }
                if (settings_open && music_toggle.contains(point)) {
                    click_sound.play();
                    music_enabled = !music_enabled;
                    if (music_enabled)
                        lobby_music.play();
                    else
                        lobby_music.pause();
                    continue;
                }
                if (settings_open && click_toggle.contains(point)) {
                    click_sound.play();
                    click_sound.set_enabled(!click_sound.enabled());
                    continue;
                }
                if (settings_open) continue;
                if (page != Page::Connection && back_button.contains(point)) {
                    click_sound.play();
                    page = page == Page::Browser ? Page::Connection : Page::Browser;
                    error.clear();
                    continue;
                }
                if (page == Page::Connection) {
                    for (std::size_t i = 0; i < connection_fields.size(); ++i)
                        if (connection_fields[i].bounds.contains(point)) active = i;
                    if (connection_button.contains(point)) {
                        click_sound.play();
                        connect();
                    }
                } else if (page == Page::Browser) {
                    if (join_button.contains(point)) {
                        click_sound.play();
                        if (lobby_created) {
                            current_players = 1;
                        } else {
                            lobby_name = "Arcade lobby";
                            lobby_size = 4;
                            current_players = 4;
                        }
                        page = Page::Waiting;
                    }
                    if (create_button.contains(point)) {
                        click_sound.play();
                        active = 0;
                        page = Page::Create;
                    }
                    if (solo_button.contains(point)) {
                        click_sound.play();
                        if (server_is_reachable(host, selected_port, error)) {
                            return ConnectionSettings{player_name, host, selected_port,
                                                      music_enabled, click_sound.enabled()};
                        }
                    }
                } else if (page == Page::Create) {
                    if (lobby_fields[0].bounds.contains(point)) active = 0;
                    if (decrease_players_button.contains(point) && lobby_size > 2) {
                        click_sound.play();
                        --lobby_size;
                    }
                    if (increase_players_button.contains(point) && lobby_size < 4) {
                        click_sound.play();
                        ++lobby_size;
                    }
                    if (make_lobby_button.contains(point)) {
                        click_sound.play();
                        if (lobby_fields[0].value.empty()) error = "Lobby name must not be empty.";
                        else {
                            lobby_name = lobby_fields[0].value;
                            current_players = 1;
                            lobby_created = true;
                            error.clear();
                            page = Page::Browser;
                        }
                    }
                } else if (page == Page::Waiting && button.contains(point) &&
                           current_players == lobby_size) {
                    click_sound.play();
                    if (server_is_reachable(host, selected_port, error)) {
                        return ConnectionSettings{player_name, host, selected_port,
                                                  music_enabled, click_sound.enabled()};
                    }
                }
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::BackSpace &&
                (page == Page::Browser || page == Page::Create || page == Page::Waiting)) {
                page = page == Page::Create || page == Page::Waiting ? Page::Browser : Page::Connection;
                error.clear();
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab &&
                page == Page::Connection)
                active = (active + 1) % connection_fields.size();
            if (event.type == sf::Event::TextEntered) {
                std::string* value = nullptr;
                if (page == Page::Connection) value = &connection_fields[active].value;
                if (page == Page::Create) value = &lobby_fields[0].value;
                if (value != nullptr) {
                    if (event.text.unicode == 8 && !value->empty()) value->pop_back();
                    else if (event.text.unicode >= 32 && event.text.unicode < 127 && value->size() < 40)
                        value->push_back(static_cast<char>(event.text.unicode));
                }
            }
        }

        window.clear(sf::Color(20, 24, 34));
        const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const bool mouse_pressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
        const sf::FloatRect view_bounds = current_view_bounds(window);
        if (page == Page::Connection) {
            draw_texture_cover(window, lobby_background, view_bounds);
            sf::RectangleShape shade({view_bounds.width, view_bounds.height});
            shade.setPosition(view_bounds.left, view_bounds.top);
            shade.setFillColor(sf::Color(8, 26, 44));
            window.draw(shade);
            draw_rounded_rect(window, {80.f, 35.f, 480.f, 355.f}, 16.f,
                              sf::Color(8, 37, 67, 220), sf::Color(99, 155, 198), 1.f);
            draw_text(window, *font, "Connexion", 213.f, 58.f, 30);
            for (std::size_t i = 0; i < connection_fields.size(); ++i) {
                const auto& field = connection_fields[i];
                draw_text(window, *font, field.label, 120.f, field.bounds.top + 20.f, 17,
                          style::muted_text);
                draw_rounded_rect(window, field.bounds, 8.f, style::field,
                                  i == active ? sf::Color(103, 177, 224) : style::border,
                                  i == active ? 2.f : 1.f);
                if (i == active) {
                    sf::RectangleShape cursor({2.f, 23.f});
                    cursor.setPosition(field.bounds.left + 15.f, field.bounds.top + 21.f);
                    cursor.setFillColor(sf::Color(146, 206, 241));
                    window.draw(cursor);
                }
                draw_text(window, *font, field.value, field.bounds.left + 24.f,
                          field.bounds.top + 19.f, 19);
            }
            draw_button(window, *font, connection_button, "Se connecter", mouse, mouse_pressed,
                        true, style::primary, style::primary_hover);
            if (error.empty())
                draw_text(window, *font, "F11 : plein ecran", 448.f, 392.f, 10,
                          style::muted_text);
        } else if (page == Page::Browser) {
            draw_texture_cover(window, lobby_background, view_bounds);
            draw_text(window, *font, "Lobbies", 228.f, 38.f, 30);
            sf::RectangleShape divider({1.f, 260.f});
            divider.setPosition(279.f, 95.f);
            divider.setFillColor(style::border);
            window.draw(divider);
            draw_text(window, *font, "Join a lobby", 65.f, 100.f, 22);
            draw_text(window, *font, "Create a lobby", 335.f, 100.f, 22);
            const sf::FloatRect lobby_bounds{45.f, 150.f, 205.f, 82.f};
            draw_rounded_rect(window, lobby_bounds, 9.f,
                              lobby_bounds.contains(mouse) ? sf::Color(28, 75, 111)
                                                           : style::panel,
                              sf::Color(78, 151, 198), 1.5f);
            draw_text(window, *font, lobby_created ? lobby_name : "Arcade lobby", 62.f, 161.f, 18);
            const std::string lobby_details = lobby_created
                ? "1 / " + std::to_string(lobby_size) + "  -  Adventure"
                : "3 / 4  -  Adventure";
            draw_text(window, *font, lobby_details, 62.f, 193.f, 13, style::muted_text);
            draw_button(window, *font, join_button, "Join selected", mouse, mouse_pressed, true,
                        style::primary, style::primary_hover);
            draw_button(window, *font, create_button, "Create lobby", mouse, mouse_pressed, true,
                        style::success, style::success_hover);
            draw_button(window, *font, solo_button, "Play solo", mouse, mouse_pressed, true,
                        style::secondary, style::secondary_hover);
        } else if (page == Page::Create) {
            draw_texture_cover(window, lobby_background, view_bounds);
            draw_text(window, *font, "Create a lobby", 178.f, 35.f, 28);
            draw_text(window, *font, "Lobby name", 45.f, 117.f, 18);
            draw_rounded_rect(window, lobby_fields[0].bounds, 8.f, style::field,
                              sf::Color(103, 177, 224), 1.5f);
            draw_text(window, *font, lobby_fields[0].value, 182.f, 115.f);
            draw_text(window, *font, "Players", 45.f, 214.f, 18);
            draw_button(window, *font, decrease_players_button, "-", mouse, mouse_pressed,
                        lobby_size > 2, style::primary, style::primary_hover);
            draw_text(window, *font, std::to_string(lobby_size), 327.f, 211.f, 24);
            draw_button(window, *font, increase_players_button, "+", mouse, mouse_pressed,
                        lobby_size < 4, style::primary, style::primary_hover);
            draw_button(window, *font, make_lobby_button, "Create lobby", mouse, mouse_pressed,
                        !lobby_fields[0].value.empty(), style::success, style::success_hover);
        } else {
            draw_texture_cover(window, lobby_background, view_bounds);
            draw_text(window, *font, "Waiting room", 190.f, 45.f, 30);
            draw_text(window, *font, lobby_name, 210.f, 105.f, 24);
            draw_text(window, *font, "Players: " + std::to_string(current_players) + " / " +
                      std::to_string(lobby_size), 210.f, 155.f, 18);
            const bool full = current_players == lobby_size;
            draw_button(window, *font, button,
                        full ? "Enter game" : "Waiting for players...", mouse, mouse_pressed,
                        full, style::success, style::success_hover);
        }
        if (page != Page::Connection)
            draw_button(window, *font, back_button, "<", mouse, mouse_pressed, true,
                        style::primary, style::primary_hover);
        draw_rounded_rect(window, settings_button, 7.f,
                          settings_button.contains(mouse) ? style::primary_hover : style::primary,
                          style::border, 1.f);
        for (int line = 0; line < 3; ++line) {
            sf::RectangleShape bar({20.f, 2.f});
            bar.setPosition(settings_button.left + 12.f,
                            settings_button.top + 10.f + static_cast<float>(line) * 7.f);
            bar.setFillColor(sf::Color::White);
            window.draw(bar);
        }
        if (settings_open) {
            sf::RectangleShape overlay({view_bounds.width, view_bounds.height});
            overlay.setPosition(view_bounds.left, view_bounds.top);
            overlay.setFillColor(sf::Color(3, 11, 20, 175));
            window.draw(overlay);
            draw_rounded_rect(window, {130.f, 90.f, 300.f, 230.f}, 12.f,
                              sf::Color(8, 31, 53, 250), style::border, 1.5f);
            draw_text(window, *font, "Reglages", 224.f, 105.f, 26);
            draw_button(window, *font, music_toggle, "Musique", mouse, mouse_pressed, true,
                        music_enabled ? style::success : style::disabled,
                        music_enabled ? style::success_hover : sf::Color(76, 88, 103));
            draw_button(window, *font, click_toggle, "Son", mouse, mouse_pressed, true,
                        click_sound.enabled() ? style::success : style::disabled,
                        click_sound.enabled() ? style::success_hover : sf::Color(76, 88, 103));
            draw_text(window, *font, "F11 : plein ecran", 215.f, 280.f, 14,
                      style::muted_text);
            // Keep the hamburger above the modal so it also closes the menu.
            draw_rounded_rect(window, settings_button, 7.f, style::primary_hover,
                              style::border, 1.f);
            for (int line = 0; line < 3; ++line) {
                sf::RectangleShape bar({20.f, 2.f});
                bar.setPosition(settings_button.left + 12.f,
                                settings_button.top + 10.f + static_cast<float>(line) * 7.f);
                bar.setFillColor(sf::Color::White);
                window.draw(bar);
            }
        }
        if (!error.empty()) {
            if (page == Page::Connection) {
                sf::RectangleShape error_background({528.f, 27.f});
                error_background.setPosition(16.f, 379.f);
                error_background.setFillColor(sf::Color(7, 37, 89, 225));
                error_background.setOutlineThickness(1.f);
                error_background.setOutlineColor(sf::Color(255, 145, 125));
                window.draw(error_background);
                draw_text(window, *font, error, 23.f, 383.f, 12, sf::Color(255, 220, 210));
            } else {
                draw_text(window, *font, error, 45.f, 382.f, 14, sf::Color(255, 120, 120));
            }
        }
        window.display();
    }
    return std::nullopt;
}

} // namespace ntype::client
