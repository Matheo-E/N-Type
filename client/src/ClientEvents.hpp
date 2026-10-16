#pragma once

#include <ntype/rtype/Components.hpp>

namespace ntype::client {

struct ReplicaSpawned {
    rtype::EntityKind kind = rtype::EntityKind::Enemy;
    rtype::Vec2f position;
};

struct ReplicaDestroyed {
    rtype::EntityKind kind = rtype::EntityKind::Enemy;
    rtype::Vec2f position;
    rtype::DestroyReason reason = rtype::DestroyReason::Despawned;
};

} // namespace ntype::client
