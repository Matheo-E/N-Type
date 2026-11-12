#pragma once

// Beams are a rule of this game, not of the engine. An attack pattern with a "beam" block in
// assets/data/attack_patterns.json is not fired as bullets: an enemy whose type names it
// ("beam_pattern" in enemies.json) casts it every "interval" seconds. The beam is one entity:
// a strip of "length" by "width" pixels starting at "origin" and going along "direction",
// harmless for "warning_time" seconds, then hurting each player inside it once (the
// pattern's "damage") during "duration" seconds. The server plays it; the client draws it
// with the pattern's sprite, stretched to the length. The engine ignores the "beam" block.

#include <cmath>
#include <fstream>
#include <map>
#include <string>

#include <SFML/System/Vector2.hpp>
#include <nlohmann/json.hpp>

namespace ntype {

struct BeamPattern {
    float interval = 8.0f;
    float warning = 1.3f;
    float duration = 1.8f;
    sf::Vector2f origin = {1280.0f, 360.0f};
    sf::Vector2f direction = {-1.0f, 0.0f}; // unit length once loaded
    float length = 1280.0f;
    float width = 96.0f;
};

// Fills `beams` with the beam patterns of an attack_patterns.json, by pattern id. Returns
// false and fills `error` if the file cannot be read or a beam's numbers make no sense.
inline bool load_beam_patterns(const std::string& path, std::map<int, BeamPattern>& beams, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    try {
        const nlohmann::json root = nlohmann::json::parse(file);
        for (const auto& item : root.at("attack_patterns")) {
            if (!item.contains("beam")) {
                continue;
            }
            const int id = item.at("id").get<int>();
            const auto& block = item.at("beam");
            BeamPattern beam;
            beam.interval = block.value("interval", beam.interval);
            beam.warning = block.value("warning_time", beam.warning);
            beam.duration = block.value("duration", beam.duration);
            beam.length = block.value("length", beam.length);
            beam.width = block.value("width", beam.width);
            if (block.contains("origin")) {
                beam.origin = {block.at("origin").at(0).get<float>(), block.at("origin").at(1).get<float>()};
            }
            if (block.contains("direction")) {
                beam.direction = {block.at("direction").at(0).get<float>(), block.at("direction").at(1).get<float>()};
            }
            const float norm = std::hypot(beam.direction.x, beam.direction.y);
            const bool valid = std::isfinite(norm) && norm > 0 && beam.interval > 0 && beam.warning >= 0 &&
                               beam.duration > 0 && beam.length > 0 && beam.width > 0;
            if (!valid) {
                error = path + ": attack pattern " + std::to_string(id) +
                        ": a beam needs a positive interval, duration, length and width, and a direction";
                return false;
            }
            beam.direction /= norm;
            beams[id] = beam;
        }
    } catch (const nlohmann::json::exception& problem) {
        error = path + ": " + problem.what();
        return false;
    }
    return true;
}

} // namespace ntype
