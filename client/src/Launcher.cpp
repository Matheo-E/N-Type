#include "Launcher.hpp"

#include <array>
#include <asio.hpp>
#include <charconv>
#include <optional>
#include <string>
#include <thread>

#include <SFML/Graphics.hpp>

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

    const auto request = protocol::makeConnectRequest("probe");
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

std::optional<ConnectionSettings> show_launcher(const ConnectionSettings& defaults) {
    auto font = load_font();
    if (!font) return std::nullopt;

    sf::RenderWindow window(sf::VideoMode(700, 520), "N-Type - Connection",
                            sf::Style::Default);
    window.setFramerateLimit(60);
    update_letterbox_view(window);
    bool fullscreen = false;
    std::array<Field, 3> connection_fields{{
        {"Name", defaults.name, {150.f, 105.f, 350.f, 48.f}},
        {"Host", defaults.host, {150.f, 175.f, 350.f, 48.f}},
        {"Port", std::to_string(defaults.port), {150.f, 245.f, 350.f, 48.f}},
    }};
    std::array<Field, 1> lobby_fields{{{"Lobby name", "My lobby", {170.f, 105.f, 330.f, 48.f}}}};
    enum class Page { Connection, Browser, Create, Waiting };
    Page page = Page::Connection;
    std::string player_name = defaults.name;
    std::string host = defaults.host;
    std::uint16_t selected_port = defaults.port;
    std::string lobby_name = "Local lobby";
    unsigned lobby_size = 4;
    unsigned current_players = 1;
    unsigned selected_map = 0;
    std::size_t active = 0;
    std::string error;
    const sf::FloatRect button{150.f, 320.f, 350.f, 52.f};
    const sf::FloatRect join_button{45.f, 250.f, 205.f, 52.f};
    const sf::FloatRect create_button{310.f, 250.f, 205.f, 52.f};
    const sf::FloatRect solo_button{150.f, 330.f, 260.f, 48.f};
    const std::array<sf::FloatRect, 3> map_cards{{
        {45.f, 190.f, 145.f, 85.f}, {207.f, 190.f, 145.f, 85.f}, {369.f, 190.f, 145.f, 85.f}}};
    const std::array<sf::Color, 3> map_colors{{
        sf::Color(55, 115, 180), sf::Color(55, 145, 85), sf::Color(175, 80, 70)}};

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
                    if (button.contains(point)) connect();
                } else if (page == Page::Browser) {
                    if (join_button.contains(point)) {
                        lobby_name = "Arcade lobby";
                        lobby_size = 4;
                        current_players = 4;
                        selected_map = 0;
                        page = Page::Waiting;
                    }
                    if (create_button.contains(point)) {
                        active = 0;
                        page = Page::Create;
                    }
                    if (solo_button.contains(point)) {
                        if (server_is_reachable(host, selected_port, error)) {
                            window.close();
                            return ConnectionSettings{player_name, host, selected_port};
                        }
                    }
                } else if (page == Page::Create) {
                    if (lobby_fields[0].bounds.contains(point)) active = 0;
                    for (unsigned i = 0; i < map_cards.size(); ++i)
                        if (map_cards[i].contains(point)) selected_map = i;
                    if (sf::FloatRect(170.f, 295.f, 70.f, 42.f).contains(point) && lobby_size > 2) --lobby_size;
                    if (sf::FloatRect(430.f, 295.f, 70.f, 42.f).contains(point) && lobby_size < 4) ++lobby_size;
                    if (sf::FloatRect(170.f, 350.f, 330.f, 45.f).contains(point)) {
                        if (lobby_fields[0].value.empty()) error = "Lobby name must not be empty.";
                        else {
                            lobby_name = lobby_fields[0].value;
                            current_players = 1;
                            error.clear();
                            page = Page::Waiting;
                        }
                    }
                } else if (page == Page::Waiting && button.contains(point) &&
                           current_players == lobby_size) {
                    if (server_is_reachable(host, selected_port, error)) {
                        window.close();
                        return ConnectionSettings{player_name, host, selected_port};
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
        draw_text(window, *font, "F11: fullscreen", 414.f, 10.f, 13, sf::Color(155, 165, 185));
        if (page == Page::Connection) {
            draw_text(window, *font, "Connect to N-Type", 150.f, 35.f, 28);
            for (std::size_t i = 0; i < connection_fields.size(); ++i) {
                const auto& field = connection_fields[i];
                draw_text(window, *font, field.label, 45.f, field.bounds.top + 10.f);
                sf::RectangleShape box({field.bounds.width, field.bounds.height});
                box.setPosition(field.bounds.left, field.bounds.top);
                box.setFillColor(sf::Color(36, 43, 58));
                box.setOutlineThickness(2.f);
                box.setOutlineColor(i == active ? sf::Color(80, 180, 255) : sf::Color(90, 98, 115));
                window.draw(box);
                draw_text(window, *font, field.value, field.bounds.left + 12.f, field.bounds.top + 10.f);
            }
            sf::RectangleShape connect_shape({button.width, button.height});
            connect_shape.setPosition(button.left, button.top);
            connect_shape.setFillColor(sf::Color(48, 145, 95));
            window.draw(connect_shape);
            draw_text(window, *font, "Continue", 275.f, 331.f, 21);
        } else if (page == Page::Browser) {
            draw_text(window, *font, "Lobbies", 228.f, 38.f, 30);
            sf::RectangleShape divider({2.f, 275.f}); divider.setPosition(279.f, 90.f);
            divider.setFillColor(sf::Color(75, 83, 100)); window.draw(divider);
            draw_text(window, *font, "Join a lobby", 65.f, 100.f, 22);
            draw_text(window, *font, "Create a lobby", 335.f, 100.f, 22);
            sf::RectangleShape lobby({205.f, 82.f}); lobby.setPosition(45.f, 150.f);
            lobby.setFillColor(sf::Color(36, 43, 58)); window.draw(lobby);
            draw_text(window, *font, "Arcade lobby", 62.f, 162.f, 19);
            draw_text(window, *font, "3 / 4  -  Blue map", 62.f, 194.f, 14, sf::Color(170, 180, 200));
            sf::RectangleShape join({join_button.width, join_button.height}); join.setPosition(join_button.left, join_button.top);
            join.setFillColor(sf::Color(55, 115, 180)); window.draw(join);
            draw_text(window, *font, "Join selected", 85.f, 263.f, 19);
            sf::RectangleShape create({create_button.width, create_button.height}); create.setPosition(create_button.left, create_button.top);
            create.setFillColor(sf::Color(48, 145, 95)); window.draw(create);
            draw_text(window, *font, "Create lobby", 350.f, 263.f, 19);
            sf::RectangleShape solo({solo_button.width, solo_button.height});
            solo.setPosition(solo_button.left, solo_button.top); solo.setFillColor(sf::Color(125, 80, 180));
            window.draw(solo); draw_text(window, *font, "Play solo", 235.f, 341.f, 20);
        } else if (page == Page::Create) {
            draw_text(window, *font, "Create a lobby", 178.f, 35.f, 28);
            draw_text(window, *font, "Backspace: back", 25.f, 12.f, 13, sf::Color(155, 165, 185));
            draw_text(window, *font, "Lobby name", 45.f, 117.f, 18);
            sf::RectangleShape name_box({330.f, 48.f}); name_box.setPosition(170.f, 105.f);
            name_box.setFillColor(sf::Color(36, 43, 58)); name_box.setOutlineThickness(2.f);
            name_box.setOutlineColor(sf::Color(80, 180, 255)); window.draw(name_box);
            draw_text(window, *font, lobby_fields[0].value, 182.f, 115.f);
            draw_text(window, *font, "Choose a map", 45.f, 165.f, 18);
            const std::array<const char*, 3> map_names{"Blue", "Green", "Red"};
            for (unsigned i = 0; i < map_cards.size(); ++i) {
                sf::RectangleShape card({map_cards[i].width, map_cards[i].height});
                card.setPosition(map_cards[i].left, map_cards[i].top); card.setFillColor(map_colors[i]);
                card.setOutlineThickness(i == selected_map ? 4.f : 0.f); card.setOutlineColor(sf::Color::White);
                window.draw(card); draw_text(window, *font, map_names[i], map_cards[i].left + 43.f, map_cards[i].top + 28.f, 18);
            }
            draw_text(window, *font, "Players", 45.f, 304.f, 18);
            draw_text(window, *font, "-", 195.f, 299.f, 26); draw_text(window, *font, std::to_string(lobby_size), 329.f, 301.f, 22);
            draw_text(window, *font, "+", 452.f, 299.f, 26);
            sf::RectangleShape make({330.f, 45.f}); make.setPosition(170.f, 350.f); make.setFillColor(sf::Color(48, 145, 95)); window.draw(make);
            draw_text(window, *font, "Create lobby", 270.f, 360.f, 19);
        } else {
            draw_text(window, *font, "Waiting room", 190.f, 45.f, 30);
            draw_text(window, *font, lobby_name, 210.f, 105.f, 24);
            draw_text(window, *font, "Players: " + std::to_string(current_players) + " / " +
                      std::to_string(lobby_size), 210.f, 155.f, 18);
            sf::RectangleShape preview({180.f, 90.f}); preview.setPosition(190.f, 195.f);
            preview.setFillColor(map_colors[selected_map]); window.draw(preview);
            draw_text(window, *font, "Map preview", 228.f, 225.f, 18);
            sf::RectangleShape start({button.width, button.height}); start.setPosition(button.left, button.top);
            const bool full = current_players == lobby_size;
            start.setFillColor(full ? sf::Color(48, 145, 95) : sf::Color(75, 80, 90)); window.draw(start);
            draw_text(window, *font, full ? "Enter game" : "Waiting for players...",
                      full ? 265.f : 218.f, 331.f, full ? 21U : 18U,
                      full ? sf::Color::White : sf::Color(175, 180, 190));
            draw_text(window, *font, "Backspace: back", 25.f, 12.f, 13, sf::Color(155, 165, 185));
        }
        if (!error.empty()) draw_text(window, *font, error, 45.f, 382.f, 14, sf::Color(255, 120, 120));
        window.display();
    }
    return std::nullopt;
}

} // namespace ntype::client
