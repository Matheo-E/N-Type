#pragma once

#include <vector>

#include <ntype/engine/math/Vec2.hpp>
#include <ntype/engine/platform/InputEvent.hpp>

namespace ntype::engine::platform {

class IWindow {
public:
    virtual ~IWindow() = default;

    /// Returns every event received since the previous call.
    [[nodiscard]] virtual std::vector<InputEvent> poll_events() = 0;
    [[nodiscard]] virtual bool is_open() const = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual math::Vec2u size() const = 0;
};

} // namespace ntype::engine::platform
