#pragma once

// Target zones are a rule of this game, not of the engine. An attack pattern marked
// "target_zone": true in assets/data/attack_patterns.json does not fire bullets: the
// server places a stationary hazard on the nearest player, harmless during
// "warning_time" seconds, then dangerous for the pattern's "lifetime". The engine ignores
// these two keys; the server plays the zones and the client draws their warning.

#include <cmath>
#include <fstream>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace ntype {

struct ZonePattern {
    float warning = 2.5f; // seconds before it hurts
};

// Fills `zones` with the target-zone patterns of an attack_patterns.json, by pattern id.
// Returns false and fills `error` if the file cannot be read, or a zone's warning_time or
// lifetime is not a positive number.
inline bool load_zone_patterns(const std::string& path, std::map<int, ZonePattern>& zones, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("attack_patterns")) {
            if (!item.value("target_zone", false)) {
                continue;
            }
            const int id = item.at("id").get<int>();
            ZonePattern zone;
            zone.warning = item.value("warning_time", zone.warning);
            const float lifetime = item.value("lifetime", 10.0f);
            if (!std::isfinite(zone.warning) || zone.warning <= 0) {
                error = path + ": attack pattern " + std::to_string(id) + ": warning_time must be positive";
                return false;
            }
            if (!std::isfinite(lifetime) || lifetime <= 0) {
                error = path + ": attack pattern " + std::to_string(id) + ": active lifetime must be positive";
                return false;
            }
            zones[id] = zone;
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    return true;
}

} // namespace ntype
