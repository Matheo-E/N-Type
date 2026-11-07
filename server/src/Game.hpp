#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "Protocol.hpp"
#include "World.hpp"

namespace ntype::server {

// One running game: one engine World, one ship per lobby slot (1 to 4).
// The server is authoritative: ships only move from the keys their player sent.
class Game {
public:
    // Spawns the ship of `slot`; does nothing if it already exists.
    void add_ship(std::uint8_t slot);
    void remove_ship(std::uint8_t slot);
    bool has_ship(std::uint8_t slot) const;
    // Ignored when `slot` has no ship.
    void set_input(std::uint8_t slot, std::uint8_t keys);
    void step(float dt);
    // One entity per ship, ordered by slot; the entity id is the slot.
    std::vector<protocol::Entity> snapshot() const;

private:
    struct Ship {
        int entity = 0;
        std::uint8_t keys = 0;
    };

    World world_;
    std::map<std::uint8_t, Ship> ships_;
};

} // namespace ntype::server
