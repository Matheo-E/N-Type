#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <ntype/engine/time/GameLoop.hpp>

using ntype::engine::time::Duration;
using ntype::engine::time::GameLoop;

TEST(GameLoop, RunsOneStepPerFixedStepOfTimeWhateverTheFrameRate) {
    GameLoop loop(Duration(1.0 / 60.0));
    int steps = 0;
    const auto count = [&](float dt) {
        EXPECT_FLOAT_EQ(dt, 1.0F / 60.0F);
        ++steps;
    };
    // One second fed in uneven frames still gives 60 steps.
    for (int i = 0; i < 50; ++i) {
        loop.advance(Duration(0.02), count);
    }
    EXPECT_EQ(steps, 60);
    EXPECT_EQ(loop.ticks(), 60U);
}

TEST(GameLoop, KeepsTheRemainderForTheNextFrame) {
    GameLoop loop(Duration(0.1));
    const auto noop = [](float) {};
    EXPECT_EQ(loop.advance(Duration(0.05), noop), 0);
    EXPECT_NEAR(loop.alpha(), 0.5F, 1e-4F);
    EXPECT_EQ(loop.advance(Duration(0.06), noop), 1);
}

TEST(GameLoop, DropsTheBacklogAfterALongStall) {
    GameLoop loop(Duration(0.01));
    EXPECT_EQ(loop.advance(Duration(10.0), [](float) {}), 8);
    EXPECT_EQ(loop.advance(Duration(0.0), [](float) {}), 0);
}

TEST(GameLoop, RunStopsFromAnotherThread) {
    GameLoop loop(Duration(0.005));
    std::jthread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.stop();
    });
    loop.run([](float) {});
    EXPECT_GT(loop.ticks(), 0U);
}
