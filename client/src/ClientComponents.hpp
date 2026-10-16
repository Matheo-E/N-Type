#pragma once

// Components that only exist on the client.

#include <ntype/rtype/Components.hpp>

namespace ntype::client {

/// Smooths a replicated entity between two server snapshots.
struct Interpolated {
    rtype::Vec2f from;
    rtype::Vec2f to;
    rtype::Vec2f velocity;
    float elapsed = 0.0F;
    float duration = 0.0F;
};

/// Display flags received in snapshots.
struct ReplicaFlags {
    bool hidden = false;
    bool invulnerable = false;
};

} // namespace ntype::client
