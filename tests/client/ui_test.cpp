#include "client/src/ui/Widgets.hpp"

#include <cassert>
#include <iostream>

using namespace ntype::client::ui;

namespace {

sf::Event press(float x, float y, sf::Mouse::Button button = sf::Mouse::Left)
{
    sf::Event event{};
    event.type = sf::Event::MouseButtonPressed;
    event.mouseButton = {button, static_cast<int>(x), static_cast<int>(y)};
    return event;
}

sf::Event typed(sf::Uint32 unicode)
{
    sf::Event event{};
    event.type = sf::Event::TextEntered;
    event.text.unicode = unicode;
    return event;
}

sf::Event wheel(float delta)
{
    sf::Event event{};
    event.type = sf::Event::MouseWheelScrolled;
    event.mouseWheelScroll = {sf::Mouse::VerticalWheel, delta, 0, 0};
    return event;
}

// Widgets receive the mouse position already mapped to the menu view.
bool click(auto& widget, float x, float y)
{
    return static_cast<bool>(widget.handle(press(x, y), {x, y}));
}

void type(TextField& field, const std::string& text)
{
    for (char c : text)
        field.handle(typed(static_cast<unsigned char>(c)), {});
}

void buttons()
{
    Button button{"Join", {10, 10, 100, 40}};
    assert(click(button, 50, 30));
    assert(!click(button, 200, 30));
    assert(!button.handle(press(50, 30, sf::Mouse::Right), {50, 30}));
    button.enabled = false;
    assert(!click(button, 50, 30));
}

void text_fields()
{
    TextField field{{0, 0, 200, 40}};
    type(field, "x");
    assert(field.value.empty());
    click(field, 10, 10);
    assert(field.focused);
    type(field, "Ann");
    assert(field.value == "Ann");
    field.handle(typed(8), {});
    assert(field.value == "An");
    field.handle(typed(233), {});   // 'é' is not printable ASCII
    field.handle(typed('\t'), {});
    assert(field.value == "An");
    assert(field.handle(typed(13), {})); // Enter submits
    field.max_length = 3;
    type(field, "nnn");
    assert(field.value == "Ann");
    click(field, 500, 500);
    assert(!field.focused);

    TextField code{{0, 0, 200, 40}, "", 4, Filter::Code};
    click(code, 1, 1);
    type(code, "k7 o");
    assert(code.value == "K7");
    type(code, "qxzz");
    assert(code.value == "K7QX");
}

void lists()
{
    ListView list{{0, 0, 400, 96}};
    list.rows = {{"a"}, {"b"}, {"c"}};
    assert(list.handle(press(10, 60), {10, 60}) == ListEvent::Selected && list.selected == 1);
    assert(list.handle(press(10, 60), {10, 60}) == ListEvent::Activated);
    assert(list.handle(press(500, 60), {500, 60}) == ListEvent::None && list.selected == 1);
    list.handle(wheel(-1), {10, 10});
    assert(list.scroll == 1);
    list.handle(wheel(-1), {10, 10}); // 3 rows, 2 visible: no further
    assert(list.scroll == 1);
    assert(list.handle(press(10, 10), {10, 10}) == ListEvent::Selected && list.selected == 1);

    ListView one{{0, 0, 400, 96}};
    one.rows = {{"a"}};
    assert(one.handle(press(10, 60), {10, 60}) == ListEvent::None && !one.selected);
}

} // namespace

int main()
{
    buttons();
    text_fields();
    lists();
    std::cout << "UI tests passed!\n";
}
