#include "../include/Input.hpp"

namespace {

// One entry per Key, in the same order as the enum.
constexpr std::array<sf::Keyboard::Key, KeyCount> Sfml = {{
    sf::Keyboard::A, sf::Keyboard::B, sf::Keyboard::C, sf::Keyboard::D, sf::Keyboard::E, sf::Keyboard::F, sf::Keyboard::G, sf::Keyboard::H, sf::Keyboard::I, sf::Keyboard::J, sf::Keyboard::K, sf::Keyboard::L, sf::Keyboard::M, sf::Keyboard::N, sf::Keyboard::O, sf::Keyboard::P, sf::Keyboard::Q, sf::Keyboard::R, sf::Keyboard::S, sf::Keyboard::T, sf::Keyboard::U, sf::Keyboard::V, sf::Keyboard::W, sf::Keyboard::X, sf::Keyboard::Y, sf::Keyboard::Z,
    sf::Keyboard::Num0, sf::Keyboard::Num1, sf::Keyboard::Num2, sf::Keyboard::Num3, sf::Keyboard::Num4, sf::Keyboard::Num5, sf::Keyboard::Num6, sf::Keyboard::Num7, sf::Keyboard::Num8, sf::Keyboard::Num9,
    sf::Keyboard::Numpad0, sf::Keyboard::Numpad1, sf::Keyboard::Numpad2, sf::Keyboard::Numpad3, sf::Keyboard::Numpad4, sf::Keyboard::Numpad5, sf::Keyboard::Numpad6, sf::Keyboard::Numpad7, sf::Keyboard::Numpad8, sf::Keyboard::Numpad9, sf::Keyboard::Add, sf::Keyboard::Subtract, sf::Keyboard::Multiply, sf::Keyboard::Divide,
    sf::Keyboard::Left, sf::Keyboard::Right, sf::Keyboard::Up, sf::Keyboard::Down,
    sf::Keyboard::Space, sf::Keyboard::Enter, sf::Keyboard::Backspace, sf::Keyboard::Tab, sf::Keyboard::Escape, sf::Keyboard::Insert, sf::Keyboard::Delete, sf::Keyboard::Home, sf::Keyboard::End, sf::Keyboard::PageUp, sf::Keyboard::PageDown, sf::Keyboard::Pause, sf::Keyboard::Menu,
    sf::Keyboard::LShift, sf::Keyboard::RShift, sf::Keyboard::LControl, sf::Keyboard::RControl, sf::Keyboard::LAlt, sf::Keyboard::RAlt, sf::Keyboard::LSystem, sf::Keyboard::RSystem,
    sf::Keyboard::LBracket, sf::Keyboard::RBracket, sf::Keyboard::Semicolon, sf::Keyboard::Comma, sf::Keyboard::Period, sf::Keyboard::Quote, sf::Keyboard::Slash, sf::Keyboard::Backslash, sf::Keyboard::Tilde, sf::Keyboard::Equal, sf::Keyboard::Hyphen,
    sf::Keyboard::F1, sf::Keyboard::F2, sf::Keyboard::F3, sf::Keyboard::F4, sf::Keyboard::F5, sf::Keyboard::F6, sf::Keyboard::F7, sf::Keyboard::F8, sf::Keyboard::F9, sf::Keyboard::F10, sf::Keyboard::F11, sf::Keyboard::F12, sf::Keyboard::F13, sf::Keyboard::F14, sf::Keyboard::F15,
}};

bool Valid(Key key) {
    return key != Key::Count;
}

std::size_t Index(Key key) {
    return static_cast<std::size_t>(key);
}

}  // namespace

sf::Keyboard::Key ToSfml(Key key) {
    return Valid(key) ? Sfml[Index(key)] : sf::Keyboard::Unknown;
}

KeyMap::KeyMap() {
    Reset();
}

bool KeyMap::IsDown(Key key) const {
    return Valid(key) && sf::Keyboard::isKeyPressed(ToSfml(Table[Index(key)]));
}

Key KeyMap::Get(Key key) const {
    return Valid(key) ? Table[Index(key)] : Key::Count;
}

void KeyMap::Set(Key key, Key physical) {
    if (Valid(key) && Valid(physical)) {
        Table[Index(key)] = physical;
    }
}

void KeyMap::Reset() {
    for (std::size_t i = 0; i < KeyCount; ++i) {
        Table[i] = static_cast<Key>(i);
    }
}

// ---- ActionMap --------------------------------------------------------------

namespace {

bool Valid(Action action) {
    return action != Action::Count;
}

std::size_t Index(Action action) {
    return static_cast<std::size_t>(action);
}

}  // namespace

ActionMap::ActionMap() {
    Reset();
}

Key ActionMap::Get(Action action) const {
    return Valid(action) ? Table[Index(action)] : Key::Count;
}

void ActionMap::Bind(Action action, Key key) {
    if (Valid(action) && Valid(key)) {
        Table[Index(action)] = key;
    }
}

// Default bindings, for an AZERTY keyboard.
void ActionMap::Reset() {
    Table.fill(Key::Count);
    Bind(Action::MoveUp, Key::Z);
    Bind(Action::MoveDown, Key::S);
    Bind(Action::MoveLeft, Key::Q);
    Bind(Action::MoveRight, Key::D);
    Bind(Action::Shoot, Key::Space);
    Bind(Action::Focus, Key::LShift);
    Bind(Action::Bomb, Key::X);
    Bind(Action::Pause, Key::Escape);
    Bind(Action::Interact, Key::E);
    Bind(Action::OpenInventory, Key::Tab);
    Bind(Action::Confirm, Key::Enter);
    Bind(Action::Cancel, Key::Escape);
    Bind(Action::MenuUp, Key::Up);
    Bind(Action::MenuDown, Key::Down);
    Bind(Action::MenuLeft, Key::Left);
    Bind(Action::MenuRight, Key::Right);
    Bind(Action::ToggleDebug, Key::F3);
}

void ActionMap::Update(const KeyMap& keys) {
    Update([&keys](Key key) { return keys.IsDown(key); });
}

void ActionMap::Update(const std::function<bool(Key)>& isdown) {
    Previous = Current;
    for (std::size_t i = 0; i < ActionCount; ++i) {
        Current[i] = Table[i] != Key::Count && isdown(Table[i]);
    }
}

bool ActionMap::IsDown(Action action) const {
    return Valid(action) && Current[Index(action)];
}

bool ActionMap::WasPressed(Action action) const {
    return Valid(action) && Current[Index(action)] && !Previous[Index(action)];
}

bool ActionMap::WasReleased(Action action) const {
    return Valid(action) && !Current[Index(action)] && Previous[Index(action)];
}
