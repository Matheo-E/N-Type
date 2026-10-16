#include <algorithm>
#include <exception>
#include <memory>
#include <string>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/View.hpp>

#include <ntype/engine/sfml/SfmlRenderer.hpp>
#include <ntype/engine/sfml/SfmlWindow.hpp>
#include <ntype/engine/util/Log.hpp>

namespace ntype::engine::sfml {

namespace {

sf::Color to_sfml(const platform::Color color) noexcept {
    return {color.r, color.g, color.b, color.a};
}

template <typename T>
bool try_load(resources::ResourceManager<T>& manager, const std::string_view id,
              const std::filesystem::path& path) {
    try {
        manager.load(std::string(id), path);
        return true;
    } catch (const std::exception& error) {
        log::error("{}", error.what());
        return false;
    }
}

} // namespace

SfmlRenderer::SfmlRenderer(SfmlWindow& window, const math::Vec2f logical_size)
    : window_(window), logical_size_(logical_size),
      textures_([](const std::filesystem::path& path) -> std::unique_ptr<sf::Texture> {
          auto texture = std::make_unique<sf::Texture>();
          if (!texture->loadFromFile(path)) {
              return nullptr;
          }
          return texture;
      }),
      fonts_([](const std::filesystem::path& path) -> std::unique_ptr<sf::Font> {
          auto font = std::make_unique<sf::Font>();
          if (!font->openFromFile(path)) {
              return nullptr;
          }
          return font;
      }) {}

bool SfmlRenderer::load_texture(const std::string_view id, const std::filesystem::path& path) {
    return try_load(textures_, id, path);
}

bool SfmlRenderer::load_font(const std::string_view id, const std::filesystem::path& path) {
    return try_load(fonts_, id, path);
}

void SfmlRenderer::update_view() {
    // Keep the logical aspect ratio and letterbox the rest of the window.
    const math::Vec2u window_size = window_.size();
    const float window_ratio =
        static_cast<float>(window_size.x) / static_cast<float>(std::max(window_size.y, 1U));
    const float logical_ratio = logical_size_.x / logical_size_.y;
    sf::FloatRect viewport({0.0F, 0.0F}, {1.0F, 1.0F});
    if (window_ratio > logical_ratio) {
        viewport.size.x = logical_ratio / window_ratio;
        viewport.position.x = (1.0F - viewport.size.x) / 2.0F;
    } else {
        viewport.size.y = window_ratio / logical_ratio;
        viewport.position.y = (1.0F - viewport.size.y) / 2.0F;
    }
    sf::View view(sf::FloatRect({0.0F, 0.0F}, {logical_size_.x, logical_size_.y}));
    view.setViewport(viewport);
    window_.native().setView(view);
}

void SfmlRenderer::begin_frame(const platform::Color clear) {
    window_.native().clear(sf::Color::Black);
    update_view();
    draw_rect({0.0F, 0.0F, logical_size_.x, logical_size_.y}, clear);
}

void SfmlRenderer::draw_sprite(const platform::SpriteDraw& sprite) {
    const sf::Texture* texture = textures_.find(std::string(sprite.texture));
    if (texture == nullptr) {
        return;
    }
    const auto& source = sprite.source;
    sf::Sprite drawable(*texture, sf::IntRect({source.x, source.y}, {source.width, source.height}));
    drawable.setOrigin(
        {static_cast<float>(source.width) / 2.0F, static_cast<float>(source.height) / 2.0F});
    drawable.setPosition({sprite.position.x, sprite.position.y});
    drawable.setScale({sprite.scale.x, sprite.scale.y});
    drawable.setColor(to_sfml(sprite.tint));
    window_.native().draw(drawable);
}

void SfmlRenderer::draw_rect(const math::Rectf& rect, const platform::Color color) {
    sf::RectangleShape shape({rect.width, rect.height});
    shape.setPosition({rect.x, rect.y});
    shape.setFillColor(to_sfml(color));
    window_.native().draw(shape);
}

void SfmlRenderer::draw_text(const std::string_view font, const std::string_view text,
                             const math::Vec2f position, const unsigned size,
                             const platform::Color color, const platform::TextAlign align) {
    const sf::Font* found = fonts_.find(std::string(font));
    if (found == nullptr) {
        return;
    }
    sf::Text drawable(*found, sf::String::fromUtf8(text.begin(), text.end()), size);
    drawable.setFillColor(to_sfml(color));
    if (align == platform::TextAlign::Center) {
        const sf::FloatRect bounds = drawable.getLocalBounds();
        drawable.setOrigin({bounds.position.x + bounds.size.x / 2.0F, 0.0F});
    }
    drawable.setPosition({position.x, position.y});
    window_.native().draw(drawable);
}

bool SfmlRenderer::save_screenshot(const std::filesystem::path& path) {
    const sf::Vector2u size = window_.native().getSize();
    sf::Texture capture(size);
    capture.update(window_.native());
    return capture.copyToImage().saveToFile(path);
}

void SfmlRenderer::end_frame() {
    window_.native().display();
}

} // namespace ntype::engine::sfml
