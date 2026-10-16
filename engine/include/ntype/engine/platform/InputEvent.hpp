#pragma once

#include <variant>

#include <ntype/engine/math/Vec2.hpp>
#include <ntype/engine/platform/Key.hpp>

namespace ntype::engine::platform {

struct WindowClosed {};
struct WindowResized {
    math::Vec2u size;
};
struct WindowFocusChanged {
    bool focused = true;
};
struct KeyPressed {
    Key key = Key::Unknown;
};
struct KeyReleased {
    Key key = Key::Unknown;
};
/// Unicode code point typed by the user (text fields).
struct TextEntered {
    char32_t codepoint = 0;
};

using InputEvent = std::variant<WindowClosed, WindowResized, WindowFocusChanged, KeyPressed,
                                KeyReleased, TextEntered>;

} // namespace ntype::engine::platform
