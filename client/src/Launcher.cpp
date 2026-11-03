#include "Launcher.hpp"

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <charconv>
#include <optional>
#include <string>
#include <thread>

#include <SFML/Audio/Music.hpp>
#include <SFML/Graphics.hpp>

#include "BackgroundConfig.hpp"
#include "Protocol.hpp"

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
    sf::Text text(value, font, size);
    text.setFillColor(color);
    text.setPosition(x, y);
    window.draw(text);
}

void draw_map_preview(sf::RenderWindow& window, const sf::Texture& texture,
                      const sf::FloatRect& bounds, const bool selected = false) {
    const auto size = texture.getSize();
    sf::Sprite preview(texture);
    preview.setPosition(bounds.left, bounds.top);
    preview.setScale(bounds.width / static_cast<float>(size.x),
                     bounds.height / static_cast<float>(size.y));
    window.draw(preview);

    sf::RectangleShape border({bounds.width, bounds.height});
    border.setPosition(bounds.left, bounds.top);
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(selected ? 4.f : 1.f);
    border.setOutlineColor(selected ? sf::Color::White : sf::Color(110, 120, 140));
    window.draw(border);
}

/**
 * @brief Draws one region of a sprite sheet inside a destination rectangle.
 *
 * The source rectangle keeps the transparent pixels surrounding each login
 * element, which preserves the soft shadows from login.png.
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
 * background.png when the launcher is resized or displayed fullscreen.
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

void update_letterbox_view(sf::RenderWindow& window) {
    constexpr float logical_width = 560.f;
    constexpr float logical_height = 410.f;
    const auto size = window.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float window_ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float logical_ratio = logical_width / logical_height;
    sf::FloatRect viewport(0.f, 0.f, 1.f, 1.f);
    if (window_ratio > logical_ratio) {
        viewport.width = logical_ratio / window_ratio;
        viewport.left = (1.f - viewport.width) / 2.f;
    } else {
        viewport.height = window_ratio / logical_ratio;
        viewport.top = (1.f - viewport.height) / 2.f;
    }
    sf::View view(sf::FloatRect(0.f, 0.f, logical_width, logical_height));
    view.setViewport(viewport);
    window.setView(view);
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
    sf::Texture adventure_preview;
    if (!adventure_preview.loadFromFile(
            background::preview_path(NTYPE_ASSET_DIR, {0, 0})))
        return std::nullopt;
    adventure_preview.setSmooth(true);
    sf::Texture login_sheet;
    sf::Texture login_background;
    const std::string asset_directory = NTYPE_ASSET_DIR;
    if (!login_sheet.loadFromFile(asset_directory + "/login.png") ||
        !login_background.loadFromFile(asset_directory + "/background.png"))
        return std::nullopt;
    login_sheet.setSmooth(true);
    login_background.setSmooth(true);
    sf::Music lobby_music;
    if (defaults.music_enabled &&
        lobby_music.openFromFile(std::string(NTYPE_ASSET_DIR) + "/music/lobby.mp3")) {
        lobby_music.setLoop(true);
        lobby_music.setVolume(40.f);
        lobby_music.play();
    }

    if (!window.isOpen()) {
        window.create(sf::VideoMode(700, 520), "N-Type - Connection", sf::Style::Default);
        fullscreen = false;
    } else {
        window.setTitle("N-Type - Connection");
        if (!fullscreen) window.setSize({700, 520});
    }
    window.setFramerateLimit(60);
    update_letterbox_view(window);
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
    std::size_t active = 0;
    std::string error;
    const sf::FloatRect button{150.f, 320.f, 350.f, 52.f};
    const sf::FloatRect connection_button{162.f, 328.f, 236.f, 46.f};
    const sf::FloatRect join_button{45.f, 250.f, 205.f, 52.f};
    const sf::FloatRect create_button{310.f, 150.f, 205.f, 46.f};
    const sf::FloatRect solo_button{310.f, 315.f, 205.f, 48.f};

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
                update_letterbox_view(window);
            }
            if (event.type == sf::Event::Resized) update_letterbox_view(window);
            if (event.type == sf::Event::MouseButtonPressed) {
                const sf::Vector2f point =
                    window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y});
                if (page == Page::Connection) {
                    for (std::size_t i = 0; i < connection_fields.size(); ++i)
                        if (connection_fields[i].bounds.contains(point)) active = i;
                    if (connection_button.contains(point)) connect();
                } else if (page == Page::Browser) {
                    if (join_button.contains(point)) {
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
                        active = 0;
                        page = Page::Create;
                    }
                    if (solo_button.contains(point)) {
                        if (server_is_reachable(host, selected_port, error)) {
                            return ConnectionSettings{player_name, host, selected_port,
                                                      defaults.music_enabled};
                        }
                    }
                } else if (page == Page::Create) {
                    if (lobby_fields[0].bounds.contains(point)) active = 0;
                    if (sf::FloatRect(170.f, 325.f, 70.f, 30.f).contains(point) && lobby_size > 2) --lobby_size;
                    if (sf::FloatRect(430.f, 325.f, 70.f, 30.f).contains(point) && lobby_size < 4) ++lobby_size;
                    if (sf::FloatRect(170.f, 350.f, 330.f, 45.f).contains(point)) {
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
                    if (server_is_reachable(host, selected_port, error)) {
                        return ConnectionSettings{player_name, host, selected_port,
                                                  defaults.music_enabled};
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
        if (page != Page::Connection)
            draw_text(window, *font, "F11: fullscreen", 414.f, 10.f, 13,
                      sf::Color(155, 165, 185));
        if (page == Page::Connection) {
            draw_texture_cover(window, login_background, {0.f, 0.f, 560.f, 410.f});
            sf::RectangleShape shade({560.f, 410.f});
            shade.setFillColor(sf::Color(8, 37, 80, 28));
            window.draw(shade);

            draw_texture_region(window, login_sheet, {40, 130, 1368, 277},
                                {30.f, 7.f, 500.f, 101.f});
            const std::array<sf::IntRect, 3> login_rows{{
                {28, 452, 1388, 180},
                {27, 645, 1390, 185},
                {26, 835, 1391, 184},
            }};
            for (std::size_t i = 0; i < connection_fields.size(); ++i) {
                const auto& field = connection_fields[i];
                draw_texture_region(window, login_sheet, login_rows[i],
                                    {20.f, field.bounds.top, 520.f, field.bounds.height});
                if (i == active) {
                    sf::RectangleShape cursor({4.f, 27.f});
                    cursor.setPosition(field.bounds.left + 17.f, field.bounds.top + 20.f);
                    cursor.setFillColor(sf::Color(255, 188, 35));
                    window.draw(cursor);
                }
                draw_text(window, *font, field.value, field.bounds.left + 29.f,
                          field.bounds.top + 19.f, 19, sf::Color(7, 37, 89));
            }
            draw_texture_region(window, login_sheet, {493, 452, 923, 180}, connection_button);
            draw_text(window, *font, "SE CONNECTER", 218.f, 339.f, 16,
                      sf::Color(7, 37, 89));
            if (error.empty())
                draw_text(window, *font, "F11 : plein ecran", 448.f, 392.f, 10,
                          sf::Color(7, 37, 89));
        } else if (page == Page::Browser) {
            draw_text(window, *font, "Lobbies", 228.f, 38.f, 30);
            sf::RectangleShape divider({2.f, 275.f}); divider.setPosition(279.f, 90.f);
            divider.setFillColor(sf::Color(75, 83, 100)); window.draw(divider);
            draw_text(window, *font, "Join a lobby", 65.f, 100.f, 22);
            draw_text(window, *font, "Create a lobby", 335.f, 100.f, 22);
            sf::RectangleShape lobby({205.f, 82.f}); lobby.setPosition(45.f, 150.f);
            lobby.setFillColor(sf::Color(36, 43, 58)); window.draw(lobby);
            draw_text(window, *font, lobby_created ? lobby_name : "Arcade lobby", 62.f, 162.f, 19);
            const std::string lobby_details = lobby_created
                ? "1 / " + std::to_string(lobby_size) + "  -  Adventure"
                : "3 / 4  -  Adventure";
            draw_text(window, *font, lobby_details, 62.f, 194.f, 14, sf::Color(170, 180, 200));
            sf::RectangleShape join({join_button.width, join_button.height}); join.setPosition(join_button.left, join_button.top);
            join.setFillColor(sf::Color(55, 115, 180)); window.draw(join);
            draw_text(window, *font, "Join selected", 85.f, 263.f, 19);
            sf::RectangleShape create({create_button.width, create_button.height}); create.setPosition(create_button.left, create_button.top);
            create.setFillColor(sf::Color(48, 145, 95)); window.draw(create);
            draw_text(window, *font, "Create lobby", 350.f, 160.f, 19);
            draw_text(window, *font, "Automatic adventure", 320.f, 205.f, 16);
            draw_map_preview(window, adventure_preview, {310.f, 230.f, 205.f, 75.f});
            draw_text(window, *font, "Levels 1 > 2 > 3", 350.f, 256.f, 14);
            sf::RectangleShape solo({solo_button.width, solo_button.height});
            solo.setPosition(solo_button.left, solo_button.top); solo.setFillColor(sf::Color(125, 80, 180));
            window.draw(solo); draw_text(window, *font, "Play solo", 370.f, 326.f, 20);
        } else if (page == Page::Create) {
            draw_text(window, *font, "Create a lobby", 178.f, 35.f, 28);
            draw_text(window, *font, "Backspace: back", 25.f, 12.f, 13, sf::Color(155, 165, 185));
            draw_text(window, *font, "Lobby name", 45.f, 117.f, 18);
            sf::RectangleShape name_box({330.f, 48.f}); name_box.setPosition(170.f, 105.f);
            name_box.setFillColor(sf::Color(36, 43, 58)); name_box.setOutlineThickness(2.f);
            name_box.setOutlineColor(sf::Color(80, 180, 255)); window.draw(name_box);
            draw_text(window, *font, lobby_fields[0].value, 182.f, 115.f);
            draw_text(window, *font, "Automatic adventure: levels 1 > 2 > 3", 90.f, 170.f, 17);
            draw_map_preview(window, adventure_preview, {170.f, 200.f, 330.f, 120.f});
            draw_text(window, *font, "Players", 45.f, 334.f, 18);
            draw_text(window, *font, "-", 195.f, 329.f, 26); draw_text(window, *font, std::to_string(lobby_size), 329.f, 331.f, 22);
            draw_text(window, *font, "+", 452.f, 329.f, 26);
            sf::RectangleShape make({330.f, 45.f}); make.setPosition(170.f, 350.f); make.setFillColor(sf::Color(48, 145, 95)); window.draw(make);
            draw_text(window, *font, "Create lobby", 270.f, 360.f, 19);
        } else {
            draw_text(window, *font, "Waiting room", 190.f, 45.f, 30);
            draw_text(window, *font, lobby_name, 210.f, 105.f, 24);
            draw_text(window, *font, "Players: " + std::to_string(current_players) + " / " +
                      std::to_string(lobby_size), 210.f, 155.f, 18);
            draw_map_preview(window, adventure_preview, {190.f, 195.f, 180.f, 90.f});
            draw_text(window, *font, "Starts at level 1", 218.f, 225.f, 16);
            sf::RectangleShape start({button.width, button.height}); start.setPosition(button.left, button.top);
            const bool full = current_players == lobby_size;
            start.setFillColor(full ? sf::Color(48, 145, 95) : sf::Color(75, 80, 90)); window.draw(start);
            draw_text(window, *font, full ? "Enter game" : "Waiting for players...",
                      full ? 265.f : 218.f, 331.f, full ? 21U : 18U,
                      full ? sf::Color::White : sf::Color(175, 180, 190));
            draw_text(window, *font, "Backspace: back", 25.f, 12.f, 13, sf::Color(155, 165, 185));
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
