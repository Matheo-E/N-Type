#include <gtest/gtest.h>

#include <ntype/engine/Version.hpp>

TEST(Version, IsNotEmpty) {
    EXPECT_FALSE(ntype::engine::version().empty());
}
