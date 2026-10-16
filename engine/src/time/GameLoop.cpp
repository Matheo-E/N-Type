#include <algorithm>
#include <thread>

#include <ntype/engine/time/GameLoop.hpp>

namespace ntype::engine::time {

namespace {

// After a long stall (debugger, window drag), drop the backlog instead of
// running hundreds of catch-up steps in a row.
constexpr int kMaxStepsPerFrame = 8;

} // namespace

GameLoop::GameLoop(const Duration fixed_step) noexcept : fixed_step_(fixed_step) {}

int GameLoop::advance(const Duration frame_time, const FixedUpdate& on_fixed_update) {
    accumulator_ += std::max(frame_time, Duration::zero());
    const auto dt = static_cast<float>(fixed_step_.count());
    int steps = 0;
    while (accumulator_ >= fixed_step_ && steps < kMaxStepsPerFrame) {
        on_fixed_update(dt);
        accumulator_ -= fixed_step_;
        ++ticks_;
        ++steps;
    }
    if (steps == kMaxStepsPerFrame && accumulator_ >= fixed_step_) {
        accumulator_ = Duration::zero();
    }
    return steps;
}

float GameLoop::alpha() const noexcept {
    return static_cast<float>(accumulator_ / fixed_step_);
}

void GameLoop::run(const FixedUpdate& on_fixed_update, const Render& on_render) {
    running_.store(true);
    Clock clock;
    while (running_.load()) {
        advance(clock.restart(), on_fixed_update);
        if (on_render) {
            on_render(alpha());
        } else {
            const Duration until_next_step = fixed_step_ - accumulator_;
            std::this_thread::sleep_for(until_next_step);
        }
    }
}

} // namespace ntype::engine::time
