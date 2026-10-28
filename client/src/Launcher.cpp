#include "Launcher.hpp"

#include <array>
#include <charconv>
#include <optional>
#include <string>

#include <SFML/Graphics.hpp>

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

} // namespace

std::optional<ConnectionSettings> show_launcher(const ConnectionSettings& defaults) {
    auto font = load_font();
    if (!font) return std::nullopt;

    sf::RenderWindow window(sf::VideoMode(700, 520), "N-Type - Connection",
                            sf::Style::Default);
    window.setFramerateLimit(60);
    update_letterbox_view(window);
    bool fullscreen = false;
    std::array<Field, 3> fields{{
        {"Name", defaults.name, {150.f, 105.f, 350.f, 48.f}},
        {"Host", defaults.host, {150.f, 175.f, 350.f, 48.f}},
        {"Port", std::to_string(defaults.port), {150.f, 245.f, 350.f, 48.f}},
    }};
    std::size_t active = 0;
    std::string error;
    const sf::FloatRect button{150.f, 320.f, 350.f, 52.f};

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
                for (std::size_t i = 0; i < fields.size(); ++i)
                    if (fields[i].bounds.contains(point)) active = i;
                if (button.contains(point)) {
                    unsigned parsed = 0;
                    const auto& port = fields[2].value;
                    const auto result = std::from_chars(port.data(), port.data() + port.size(), parsed);
                    if (fields[0].value.empty() || fields[1].value.empty() || result.ec != std::errc{} ||
                        result.ptr != port.data() + port.size() || parsed == 0 || parsed > 65535) {
                        error = "Please enter a name, host and a valid port.";
                    } else {
                        window.close();
                        return ConnectionSettings{fields[0].value, fields[1].value,
                                                  static_cast<std::uint16_t>(parsed)};
                    }
                }
            }
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Tab)
                active = (active + 1) % fields.size();
            if (event.type == sf::Event::TextEntered) {
                auto& value = fields[active].value;
                if (event.text.unicode == 8 && !value.empty()) value.pop_back();
                else if (event.text.unicode >= 32 && event.text.unicode < 127 && value.size() < 40)
                    value.push_back(static_cast<char>(event.text.unicode));
            }
        }

        window.clear(sf::Color(20, 24, 34));
        draw_text(window, *font, "Connect to N-Type", 150.f, 35.f, 28);
        draw_text(window, *font, "F11: fullscreen", 414.f, 10.f, 13, sf::Color(155, 165, 185));
        for (std::size_t i = 0; i < fields.size(); ++i) {
            draw_text(window, *font, fields[i].label, 45.f, fields[i].bounds.top + 10.f);
            sf::RectangleShape box({fields[i].bounds.width, fields[i].bounds.height});
            box.setPosition(fields[i].bounds.left, fields[i].bounds.top);
            box.setFillColor(sf::Color(36, 43, 58));
            box.setOutlineThickness(2.f);
            box.setOutlineColor(i == active ? sf::Color(80, 180, 255) : sf::Color(90, 98, 115));
            window.draw(box);
            draw_text(window, *font, fields[i].value, fields[i].bounds.left + 12.f,
                      fields[i].bounds.top + 10.f);
        }
        sf::RectangleShape connect({button.width, button.height});
        connect.setPosition(button.left, button.top);
        connect.setFillColor(sf::Color(48, 145, 95));
        window.draw(connect);
        draw_text(window, *font, "Connect", 285.f, 331.f, 21);
        if (!error.empty()) draw_text(window, *font, error, 150.f, 382.f, 14, sf::Color(255, 120, 120));
        window.display();
    }
    return std::nullopt;
}

} // namespace ntype::client
