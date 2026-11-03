#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace ntype::client::background {

inline constexpr std::size_t kLevelCount = 3;
inline constexpr std::size_t kAmbienceCount = 2;
inline constexpr std::size_t kLayerCount = 4;
inline constexpr float kDayNightPeriodSeconds = 60.f;
inline constexpr float kTemporaryLevelDurationSeconds = 120.f;
/** Duration of the smooth crossfade between day and night backgrounds. */
inline constexpr float kDayNightFadeDurationSeconds = 6.f;
static_assert(kDayNightFadeDurationSeconds > 0.f &&
              kDayNightFadeDurationSeconds < kDayNightPeriodSeconds);

inline constexpr std::array<const char*, kLevelCount> kLevelFolders{
    "level_01_ice", "level_02_candy", "level_03_dentist_lab"};
inline constexpr std::array<const char*, kLevelCount> kMusicFiles{
    "level_01_ice.mp3", "level_02_candy.mp3", "level_03_dentist_lab.mp3"};
inline constexpr std::array<const char*, kAmbienceCount> kAmbienceFolders{"day", "night"};
inline constexpr std::array<const char*, kLayerCount> kLayerFiles{
    "layer_01_far.png", "layer_02_middle.png", "layer_03_near.png",
    "layer_04_ground.png"};
/** Preview filenames preserve the actual format available for each ambience. */
inline constexpr std::array<std::array<const char*, kAmbienceCount>, kLevelCount>
    kPreviewFiles{{
        {"preview.png", "preview.png"},
        {"preview.png", "preview.png"},
        {"preview.jpg", "preview.png"},
    }};

/**
 * @brief Base speed of the automatic background scroll, in pixels per second.
 *
 * Each layer multiplies this speed by its Parallax factor below. Set this value
 * to zero to restore a background driven only by the player camera.
 */
inline constexpr float kAutomaticScrollSpeed = 100.f;
static_assert(kAutomaticScrollSpeed >= 0.f);

/**
 * @brief Relative scrolling speed of each layer, from distant scenery to ground.
 *
 * A value of 1 follows the camera exactly. Smaller values create depth by moving
 * more slowly. These are the only values to change when tuning the parallax.
 */
inline constexpr std::array<float, kLayerCount> kParallaxFactors{0.08f, 0.25f, 0.55f, 1.0f};

struct Selection {
    std::size_t level = 0;
    std::size_t ambience = 0;
};

/**
 * @brief Computes the eased opacity of the incoming day/night background.
 * @param elapsed_seconds Time elapsed since the crossfade started.
 * @return Smoothstep progress clamped between zero and one.
 */
inline constexpr float day_night_fade_progress(const float elapsed_seconds) {
    const float linear_progress = std::clamp(
        elapsed_seconds / kDayNightFadeDurationSeconds, 0.f, 1.f);
    return linear_progress * linear_progress * (3.f - 2.f * linear_progress);
}

static_assert(day_night_fade_progress(-1.f) == 0.f);
static_assert(day_night_fade_progress(kDayNightFadeDurationSeconds / 2.f) == 0.5f);
static_assert(day_night_fade_progress(kDayNightFadeDurationSeconds) == 1.f);

/**
 * @brief Selects the automatic level and ambience for an elapsed game time.
 *
 * The temporary test configuration advances levels every two minutes until
 * level 3 and alternates day/night every minute. These timings remain
 * independent of image width and can be restored for production above.
 * @param elapsed_seconds Time elapsed since the current game session started.
 * @return Level and ambience that must currently be displayed.
 */
inline constexpr Selection selection_at_time(const float elapsed_seconds) {
    const float safe_time = std::max(0.f, elapsed_seconds);
    const auto level = std::min(
        static_cast<std::size_t>(safe_time / kTemporaryLevelDurationSeconds),
        kLevelCount - 1);
    const auto ambience = static_cast<std::size_t>(safe_time / kDayNightPeriodSeconds) %
                          kAmbienceCount;
    return {level, ambience};
}

static_assert(selection_at_time(59.9f).ambience == 0);
static_assert(selection_at_time(60.f).ambience == 1);
static_assert(selection_at_time(120.f).level == 1);
static_assert(selection_at_time(120.f).ambience == 0);
static_assert(selection_at_time(240.f).level == 2);

/**
 * @brief Builds the path of one of the 24 parallax images.
 * @param asset_root Root of the organized assets directory.
 * @param selection Selected level and day/night ambience.
 * @param layer Layer index: 0 distant, 1 middle, 2 near, 3 ground.
 * @return Absolute path to the existing PNG asset.
 */
inline std::string layer_path(const std::string& asset_root, const Selection selection,
                              const std::size_t layer) {
    return asset_root + "/backgrounds/" + kLevelFolders[selection.level] + "/" +
           kAmbienceFolders[selection.ambience] + "/" + kLayerFiles[layer];
}

/**
 * @brief Builds the preview path for a level and ambience.
 * @param asset_root Root of the organized assets directory.
 * @param selection Selected level and day/night ambience.
 * @return Absolute path to the existing PNG or JPEG preview.
 */
inline std::string preview_path(const std::string& asset_root, const Selection selection) {
    return asset_root + "/backgrounds/" + kLevelFolders[selection.level] + "/" +
           kAmbienceFolders[selection.ambience] + "/" +
           kPreviewFiles[selection.level][selection.ambience];
}

/**
 * @brief Builds the music path associated with the selected level.
 * @param asset_root Root of the organized assets directory.
 * @param selection Selected level; its ambience does not affect the music.
 * @return Absolute path to the level soundtrack.
 */
inline std::string music_path(const std::string& asset_root, const Selection selection) {
    return asset_root + "/audio/music/" + kMusicFiles[selection.level];
}

} // namespace ntype::client::background
