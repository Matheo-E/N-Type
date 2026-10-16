#include <system_error>
#include <vector>

#include <ntype/rtype/Assets.hpp>

namespace ntype::rtype {

namespace {

bool looks_like_assets(const std::filesystem::path& directory) {
    std::error_code error;
    return std::filesystem::is_regular_file(directory / "data" / "game.json", error);
}

} // namespace

std::optional<std::filesystem::path>
find_assets_directory(const std::filesystem::path& explicit_directory, const char* argv0) {
    if (!explicit_directory.empty()) {
        if (looks_like_assets(explicit_directory)) {
            return explicit_directory;
        }
        return std::nullopt;
    }
    std::vector<std::filesystem::path> candidates;
    std::error_code error;
    candidates.push_back(std::filesystem::current_path(error) / "assets");
    if (argv0 != nullptr) {
        std::filesystem::path directory =
            std::filesystem::weakly_canonical(std::filesystem::path(argv0), error).parent_path();
        // bin/ -> build/<preset>/ -> build/ -> repository root
        for (int depth = 0; depth < 4 && !directory.empty(); ++depth) {
            candidates.push_back(directory / "assets");
            directory = directory.parent_path();
        }
    }
    for (const auto& candidate : candidates) {
        if (looks_like_assets(candidate)) {
            return candidate.lexically_normal();
        }
    }
    return std::nullopt;
}

} // namespace ntype::rtype
