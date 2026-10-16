#pragma once

#include <SFML/Window/Keyboard.hpp>

#include <ntype/engine/platform/Key.hpp>

namespace ntype::engine::sfml {

[[nodiscard]] platform::Key from_sfml(sf::Keyboard::Key key) noexcept;
[[nodiscard]] sf::Keyboard::Key to_sfml(platform::Key key) noexcept;

} // namespace ntype::engine::sfml
