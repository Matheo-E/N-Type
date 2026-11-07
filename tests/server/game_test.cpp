#include "server/src/Game.hpp"

#include <cassert>
#include <iostream>

using ntype::server::Game;
using protocol::InputType;

namespace {

std::uint8_t keys(InputType a, InputType b)
{
    return static_cast<std::uint8_t>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

bool at(const protocol::Entity& entity, std::uint16_t slot, std::int32_t x, std::int32_t y)
{
    return entity.id == slot && entity.kind == 1 && entity.x == x && entity.y == y;
}

} // namespace

int main()
{
    Game game;
    game.add_ship(1);
    game.add_ship(3);
    std::vector<protocol::Entity> ships = game.snapshot();
    assert(ships.size() == 2 && at(ships[0], 1, 100, 150) && at(ships[1], 3, 100, 390));

    // Ships move at 300 px/s and only with their own keys.
    game.set_input(1, static_cast<std::uint8_t>(InputType::Right));
    game.step(1.0F);
    ships = game.snapshot();
    assert(at(ships[0], 1, 400, 150) && at(ships[1], 3, 100, 390));

    // Positions stay inside the 1280x720 world (40x40 ships).
    game.set_input(1, keys(InputType::Up, InputType::Left));
    game.set_input(3, keys(InputType::Down, InputType::Right));
    game.step(10.0F);
    ships = game.snapshot();
    assert(at(ships[0], 1, 0, 0) && at(ships[1], 3, 1240, 680));

    game.remove_ship(1);
    assert(!game.has_ship(1) && game.has_ship(3));
    game.set_input(2, static_cast<std::uint8_t>(InputType::Right)); // no ship: ignored
    game.step(1.0F);
    ships = game.snapshot();
    assert(ships.size() == 1 && ships[0].id == 3);

    game.add_ship(3); // already there: unchanged
    assert(game.snapshot().size() == 1 && at(game.snapshot()[0], 3, 1240, 680));

    std::cout << "Game tests passed!\n";
}
