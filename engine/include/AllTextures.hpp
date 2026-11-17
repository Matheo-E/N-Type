#pragma once

#include "SFML/Graphics.hpp"

#include <string>

// One entry of assets/data/textures.json: an image loaded once and shared.
// Entities never hold the image, only its id (see ESprite).
class AllTexture {
public:
    std::string Path;  // file the image was loaded from, kept for error messages
    sf::Texture Image;
};
