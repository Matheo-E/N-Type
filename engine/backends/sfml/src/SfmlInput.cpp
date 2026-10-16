#include <ntype/engine/sfml/KeyMapping.hpp>
#include <ntype/engine/sfml/SfmlInput.hpp>
#include <ntype/engine/sfml/SfmlWindow.hpp>

namespace ntype::engine::sfml {

bool SfmlInput::is_key_down(const platform::Key key) const {
    const sf::Keyboard::Key native = to_sfml(key);
    return window_.has_focus() && native != sf::Keyboard::Key::Unknown &&
           sf::Keyboard::isKeyPressed(native);
}

} // namespace ntype::engine::sfml
