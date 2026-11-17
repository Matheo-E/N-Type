#pragma once

#include <array>
#include <cstddef>
#include <functional>

#include "SFML/Window.hpp"

// Every keyboard key, under the engine's own names. The game only ever talks about
// Key::Z, never about an SFML key, so the keyboard layer can change in one place.
enum class Key {
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Digits
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4, Numpad5, Numpad6, Numpad7, Numpad8, Numpad9, Add, Subtract, Multiply, Divide,
    // Arrows
    Left, Right, Up, Down,
    // Editing
    Space, Enter, Backspace, Tab, Escape, Insert, Delete, Home, End, PageUp, PageDown, Pause, Menu,
    // Modifiers
    LShift, RShift, LControl, RControl, LAlt, RAlt, LSystem, RSystem,
    // Symbols
    LBracket, RBracket, Semicolon, Comma, Period, Quote, Slash, Backslash, Tilde, Equal, Hyphen,
    // Function
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15,
    Count  // not a key: the number of keys
};

inline constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);

// The SFML key behind a Key, using the default layout (Key::Z is the Z key).
sf::Keyboard::Key ToSfml(Key key);

// Says which physical key each Key reads. At the start every Key reads itself.
// Remapping is changing what a Key reads: after Set(Key::Z, Key::W), IsDown(Key::Z)
// is true while the W key is held. The rest of the program is untouched.
class KeyMap {
public:
    KeyMap();

    // Is the physical key behind this Key held right now?
    bool IsDown(Key key) const;

    // The physical key this Key currently reads.
    Key Get(Key key) const;

    // Makes this Key read another physical key. Ignores Key::Count.
    void Set(Key key, Key physical);

    // Back to the defaults: every Key reads itself.
    void Reset();

private:
    std::array<Key, KeyCount> Table;
};

// ---------------------------------------------------------------------------
// Actions: what the player wants to do, independent of which key does it.
// The program asks "is MoveUp held?", never "is Z held?". Rebinding an action
// (an options menu) only changes this table.
//
//   Action  ->  Key  ->  physical key
//          ActionMap   KeyMap
// ---------------------------------------------------------------------------

enum class Action {
    // Movement
    MoveUp, MoveDown, MoveLeft, MoveRight,
    // Combat
    Shoot, Focus, Bomb,  // Focus: slow down for precise dodging
    // In-game
    Pause, Interact, OpenInventory,
    // Menus
    Confirm, Cancel, MenuUp, MenuDown, MenuLeft, MenuRight,
    // Debug
    ToggleDebug,
    Count  // not an action: the number of actions
};

inline constexpr std::size_t ActionCount = static_cast<std::size_t>(Action::Count);

class ActionMap {
public:
    ActionMap();

    // The Key an action is bound to.
    Key Get(Action action) const;

    // Binds an action to a Key. Several actions may share a key (Pause and Cancel
    // are both Escape): they are used in different places. Ignores Action::Count.
    void Bind(Action action, Key key);

    // Back to the default bindings.
    void Reset();

    // Call once per frame, before reading actions. Reads the keyboard through the
    // KeyMap. The overload taking a function is for tests and replays.
    void Update(const KeyMap& keys);
    void Update(const std::function<bool(Key)>& isdown);

    bool IsDown(Action action) const;       // held right now
    bool WasPressed(Action action) const;   // went down this frame: use for pause, menus
    bool WasReleased(Action action) const;  // went up this frame

private:
    std::array<Key, ActionCount> Table;
    std::array<bool, ActionCount> Current{};
    std::array<bool, ActionCount> Previous{};
};
