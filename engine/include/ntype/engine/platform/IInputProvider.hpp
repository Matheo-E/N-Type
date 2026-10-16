#pragma once

#include <ntype/engine/platform/Key.hpp>

namespace ntype::engine::platform {

/// Real-time input state (as opposed to the event queue of IWindow).
class IInputProvider {
public:
    virtual ~IInputProvider() = default;

    [[nodiscard]] virtual bool is_key_down(Key key) const = 0;
};

} // namespace ntype::engine::platform
