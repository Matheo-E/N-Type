#pragma once

#include <filesystem>

#include <ntype/rtype/GameData.hpp>

/// The real game data shipped in assets/, so tests exercise the actual content.
inline const ntype::rtype::GameData& test_game_data() {
    static const auto data = ntype::rtype::GameData::load_from_file(
        std::filesystem::path(NTYPE_TEST_ASSETS_DIR) / "data" / "game.json");
    return data;
}
