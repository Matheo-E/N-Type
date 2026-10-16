#pragma once

#include <string>

#include <SFML/Graphics/RenderWindow.hpp>

#include <ntype/engine/platform/IWindow.hpp>

namespace ntype::engine::sfml {

class SfmlWindow final : public platform::IWindow {
public:
    SfmlWindow(math::Vec2u size, const std::string& title);

    [[nodiscard]] std::vector<platform::InputEvent> poll_events() override;
    [[nodiscard]] bool is_open() const override;
    void close() override;
    [[nodiscard]] math::Vec2u size() const override;

    [[nodiscard]] bool has_focus() const;
    /// Backend-internal access for the renderer.
    [[nodiscard]] sf::RenderWindow& native() noexcept { return window_; }

private:
    sf::RenderWindow window_;
};

} // namespace ntype::engine::sfml
