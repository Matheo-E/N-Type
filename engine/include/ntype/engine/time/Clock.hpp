#pragma once

#include <chrono>

namespace ntype::engine::time {

using SteadyClock = std::chrono::steady_clock;
using TimePoint = SteadyClock::time_point;
using Duration = std::chrono::duration<double>;

/// Measures elapsed wall time with a monotonic clock.
class Clock {
public:
    Clock() noexcept : last_(SteadyClock::now()) {}

    /// Returns the time since the last restart and starts over.
    Duration restart() noexcept {
        const TimePoint now = SteadyClock::now();
        const Duration elapsed = now - last_;
        last_ = now;
        return elapsed;
    }

    [[nodiscard]] Duration elapsed() const noexcept { return SteadyClock::now() - last_; }

private:
    TimePoint last_;
};

} // namespace ntype::engine::time
