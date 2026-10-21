#include "common/protocol/Protocol.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace protocol;

    auto request = decode(makeConnectRequest());
    assert(request && request->type == MessageType::ConnectRequest);
    auto accepted = decode(makeConnectAccept(2));
    assert(accepted && accepted->id == 2);
    auto refused = decode(makeConnectRefuse(RefuseReason::ServerFull));
    assert(refused && refused->value == 1);
    assert(decode(makeDisconnect()));

    uint8_t keys = static_cast<uint8_t>(InputType::Right) |
                   static_cast<uint8_t>(InputType::Shoot);
    auto input = decode(makeInput(keys));
    assert(input && input->value == 24);

    auto update = decode(makeEntityUpdate(250, -30, 450));
    assert(update && update->id == 250 && update->x == -30 && update->y == 450);

    auto spawn = decode(makeEntitySpawn(251, 2, 800, 200));
    assert(spawn && spawn->id == 251 && spawn->value == 2 && spawn->x == 800);

    auto destroy = decode(makeEntityDestroy(251));
    assert(destroy && destroy->id == 251);

    std::vector<Entity> entities = {{1, 1, 100, 200}, {251, 2, -30, 450}};
    auto snapshot = decode(makeWorldSnapshot(entities));
    assert(snapshot && snapshot->entities.size() == 2);
    assert(snapshot->entities[1].id == 251 && snapshot->entities[1].x == -30);
    assert(decode(makeWorldSnapshot({}))->entities.empty());

    auto event = decode(makePlayerEvent(2, 1));
    assert(event && event->id == 2 && event->value == 1);
    auto left = decode(makeClientLeft(2));
    assert(left && left->id == 2);
    auto state = decode(makeGameStateChange(2));
    assert(state && state->value == 2);

    assert(!decode({}));
    assert(!decode({99}));
    assert(!decode({6, 1, 2}));
    assert(!decode({5, 255}));
    assert(!decode({9, 1}));
    assert(!decode({12, 99}));

    bool rejected = false;
    try { makeWorldSnapshot(std::vector<Entity>(101)); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);

    std::cout << "All 12 protocol messages passed!\n";
}
