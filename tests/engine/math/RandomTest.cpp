#include <gtest/gtest.h>

#include <ntype/engine/math/Random.hpp>
#include <ntype/engine/math/Rect.hpp>

using ntype::engine::math::Random;
using ntype::engine::math::Rectf;

TEST(Random, SameSeedSameSequence) {
    Random a(1234);
    Random b(1234);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(a.next_u32(), b.next_u32());
    }
}

TEST(Random, StaysWithinBounds) {
    Random rng(7);
    for (int i = 0; i < 1000; ++i) {
        const float value = rng.next_float(-2.0F, 3.0F);
        EXPECT_GE(value, -2.0F);
        EXPECT_LT(value, 3.0F);
        const int integer = rng.next_int(5, 9);
        EXPECT_GE(integer, 5);
        EXPECT_LE(integer, 9);
    }
}

TEST(Rect, IntersectsExcludesTouchingEdges) {
    const Rectf a{0, 0, 10, 10};
    EXPECT_TRUE(a.intersects({5, 5, 10, 10}));
    EXPECT_FALSE(a.intersects({10, 0, 10, 10}));
    EXPECT_EQ(Rectf::centered({5, 5}, {10, 10}), a);
}
