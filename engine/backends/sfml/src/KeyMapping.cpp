#include <utility>

#include <ntype/engine/sfml/KeyMapping.hpp>

namespace ntype::engine::sfml {

namespace {

using platform::Key;
using SfKey = sf::Keyboard::Key;

constexpr std::pair<Key, SfKey> kSpecialKeys[] = {
    {Key::Up, SfKey::Up},
    {Key::Down, SfKey::Down},
    {Key::Left, SfKey::Left},
    {Key::Right, SfKey::Right},
    {Key::Space, SfKey::Space},
    {Key::Enter, SfKey::Enter},
    {Key::Escape, SfKey::Escape},
    {Key::Backspace, SfKey::Backspace},
    {Key::Tab, SfKey::Tab},
    {Key::LShift, SfKey::LShift},
    {Key::RShift, SfKey::RShift},
    {Key::LControl, SfKey::LControl},
    {Key::RControl, SfKey::RControl},
    {Key::F1, SfKey::F1},
    {Key::F2, SfKey::F2},
    {Key::F3, SfKey::F3},
};

// Letters and digits are contiguous in both enums.
template <typename To, typename From>
To offset(const From first_to, const From value, const To first) {
    return static_cast<To>(static_cast<int>(first) +
                           (static_cast<int>(value) - static_cast<int>(first_to)));
}

} // namespace

Key from_sfml(const SfKey key) noexcept {
    const int code = static_cast<int>(key);
    if (code >= static_cast<int>(SfKey::A) && code <= static_cast<int>(SfKey::Z)) {
        return offset(SfKey::A, key, Key::A);
    }
    if (code >= static_cast<int>(SfKey::Num0) && code <= static_cast<int>(SfKey::Num9)) {
        return offset(SfKey::Num0, key, Key::Num0);
    }
    for (const auto& [ours, theirs] : kSpecialKeys) {
        if (theirs == key) {
            return ours;
        }
    }
    return Key::Unknown;
}

SfKey to_sfml(const Key key) noexcept {
    const int code = static_cast<int>(key);
    if (code >= static_cast<int>(Key::A) && code <= static_cast<int>(Key::Z)) {
        return offset(Key::A, key, SfKey::A);
    }
    if (code >= static_cast<int>(Key::Num0) && code <= static_cast<int>(Key::Num9)) {
        return offset(Key::Num0, key, SfKey::Num0);
    }
    for (const auto& [ours, theirs] : kSpecialKeys) {
        if (ours == key) {
            return theirs;
        }
    }
    return SfKey::Unknown;
}

} // namespace ntype::engine::sfml
