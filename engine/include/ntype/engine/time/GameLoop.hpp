#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include <ntype/engine/time/Clock.hpp>

namespace ntype::engine::time {

/// Fixed-timestep loop: the simulation always advances by the same `dt`,
/// whatever the speed of the machine, while rendering runs as often as it can.
class GameLoop {
public:
    using FixedUpdate = std::function<void(float dt)>;
    /// `alpha` in [0, 1): how far the frame is between two fixed steps.
    using Render = std::function<void(float alpha)>;

    explicit GameLoop(Duration fixed_step) noexcept;

    /// Blocks until stop(). Without a render callback, the loop sleeps until
    /// the next step instead of spinning (server mode).
    void run(const FixedUpdate& on_fixed_update, const Render& on_render = {});
    /// Thread-safe.
    void stop() noexcept { running_.store(false); }
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

    /// Feeds `frame_time` into the accumulator and runs the due fixed steps.
    /// Returns how many ran. Exposed so the timing logic is testable.
    int advance(Duration frame_time, const FixedUpdate& on_fixed_update);

    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] Duration fixed_step() const noexcept { return fixed_step_; }
    [[nodiscard]] float alpha() const noexcept;

private:
    Duration fixed_step_;
    Duration accumulator_{0.0};
    std::atomic<bool> running_{false};
    std::uint64_t ticks_ = 0;
};

} // namespace ntype::engine::time
