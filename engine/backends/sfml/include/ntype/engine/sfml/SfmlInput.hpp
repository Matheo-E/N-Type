#pragma once

#include <ntype/engine/platform/IInputProvider.hpp>

namespace ntype::engine::sfml {

class SfmlWindow;

/// Keyboard state, ignored while the window does not have the focus.
class SfmlInput final : public platform::IInputProvider {
public:
    explicit SfmlInput(const SfmlWindow& window) noexcept : window_(window) {}

    [[nodiscard]] bool is_key_down(platform::Key key) const override;

private:
    const SfmlWindow& window_;
};

} // namespace ntype::engine::sfml
