#include <stdexcept>

#include <gtest/gtest.h>

#include "TestData.hpp"

using ntype::rtype::GameData;

TEST(GameData, LoadsTheShippedFile) {
    const GameData& data = test_game_data();
    EXPECT_EQ(data.player.sprites.size(), 4U);
    EXPECT_FALSE(data.enemies.empty());
    EXPECT_FALSE(data.waves.empty());
    EXPECT_NE(data.find_projectile(data.player.projectile), nullptr);
    for (const auto& wave : data.waves) {
        EXPECT_NE(data.find_enemy(wave.enemy), nullptr);
    }
}

TEST(GameData, ReportsInvalidContent) {
    EXPECT_THROW(static_cast<void>(GameData::load_from_string("{")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(GameData::load_from_string("{}")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(GameData::load_from_file("does/not/exist.json")),
                 std::runtime_error);
}
