#include "InputMapper.hpp"

#include <initializer_list>

#include <ntype/rtype/Components.hpp>

namespace ntype::client {

namespace {

using engine::platform::Key;

bool any_down(const engine::platform::IInputProvider& input,
              const std::initializer_list<Key> keys) {
    for (const Key key : keys) {
        if (input.is_key_down(key)) {
            return true;
        }
    }
    return false;
}

} // namespace

std::uint8_t InputMapper::sample(const engine::platform::IInputProvider& input) const {
    namespace bits = rtype::input_bits;
    std::uint8_t result = 0;
    if (any_down(input, {Key::Up, Key::Z, Key::W})) {
        result |= bits::kUp;
    }
    if (any_down(input, {Key::Down, Key::S})) {
        result |= bits::kDown;
    }
    if (any_down(input, {Key::Left, Key::Q, Key::A})) {
        result |= bits::kLeft;
    }
    if (any_down(input, {Key::Right, Key::D})) {
        result |= bits::kRight;
    }
    if (any_down(input, {Key::Space})) {
        result |= bits::kFire;
    }
    return result;
}

} // namespace ntype::client
