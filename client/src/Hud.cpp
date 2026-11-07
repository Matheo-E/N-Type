#include "Hud.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/View.hpp>

#include "GameConfig.hpp"
#include "PlayerColors.hpp"

namespace ntype::client {

namespace {

// FreeGUI pieces, as rectangles of the original sheets.
const sf::IntRect kTile{1181, 668, 177, 183};      // Button.png: grey glossy square, tinted per player
const sf::IntRect kPlate{3468, 4426, 444, 99};     // Window.png: framed cream bar
constexpr int kPlateCap = 46;                      // its rounded ends, kept unstretched
const sf::IntRect kInfoPlate{3534, 4690, 357, 57}; // Window.png: plain cream bar
constexpr int kInfoCap = 20;
const sf::IntRect kStar{1831, 4814, 129, 123};     // Window.png: yellow star
// The tooth inside the first frame of sprites/player/tooth.png, without its margins.
const sf::IntRect kPortrait{40, 100, 660, 500};

const sf::Color kInk{85, 49, 39};       // same browns as the menus (GuiSkin.hpp)
const sf::Color kMuted{133, 95, 66};

constexpr float kRowHeight = 46.f; // card height, in game units (the play area is 1280x720)
constexpr std::size_t kScoreDigits = 6;

void draw_region(sf::RenderTarget& target, const sf::Texture& texture, const sf::IntRect& source,
                 const sf::FloatRect& bounds, const sf::Color& colour = sf::Color::White) {
    sf::Sprite sprite(texture, source);
    sprite.setPosition(bounds.left, bounds.top);
    sprite.setScale(bounds.width / static_cast<float>(source.width),
                    bounds.height / static_cast<float>(source.height));
    sprite.setColor(colour);
    target.draw(sprite);
}

// Recolours the grey tile, keeping its shading: grey 153 (the body) becomes the colour,
// darker greys a darker shade, lighter greys move towards white (gloss, border).
sf::Color recolour(const sf::Uint8 grey, const sf::Color& colour) {
    const float g = static_cast<float>(grey);
    const auto channel = [g](const sf::Uint8 c) {
        const float value = static_cast<float>(c);
        if (g < 153.f) return value * std::max(0.6f, 0.75f + 0.25f * (g - 120.f) / 33.f);
        const float white = g <= 200.f ? (g - 153.f) / 47.f * 0.3f : 0.3f + (g - 200.f) / 50.f * 0.7f;
        return value + (255.f - value) * std::min(1.f, white);
    };
    return {static_cast<sf::Uint8>(channel(colour.r)), static_cast<sf::Uint8>(channel(colour.g)),
            static_cast<sf::Uint8>(channel(colour.b))};
}

// A deeper shade of the slot colour, readable on the cream plates.
sf::Color text_colour(const sf::Color& colour) {
    return {static_cast<sf::Uint8>(colour.r * 4 / 5), static_cast<sf::Uint8>(colour.g * 4 / 5),
            static_cast<sf::Uint8>(colour.b * 4 / 5)};
}

sf::Text make_text(const sf::Font& font, const sf::String& value, const float size, const sf::Color& fill) {
    sf::Text text(value, font, static_cast<unsigned>(std::max(9.f, std::round(size))));
    text.setStyle(sf::Text::Bold);
    text.setFillColor(fill);
    return text;
}

// Puts the text's vertical middle (measured on capitals) at `middle`, pixel-aligned.
void place(sf::Text& text, const sf::Font& font, const float x, const float middle) {
    const sf::Text reference("H", font, text.getCharacterSize());
    const sf::FloatRect cap = reference.getLocalBounds();
    text.setPosition(std::round(x), std::round(middle - cap.top - cap.height / 2.f));
}

// A health bar: a dark track, filled from the left with `fill`.
void draw_bar(sf::RenderTarget& target, const sf::FloatRect& bounds, const float ratio, const sf::Color& fill) {
    sf::RectangleShape track({bounds.width, bounds.height});
    track.setPosition(bounds.left, bounds.top);
    track.setFillColor(sf::Color(85, 49, 39, 110));
    target.draw(track);
    sf::RectangleShape level({std::round(bounds.width * std::clamp(ratio, 0.f, 1.f)), bounds.height});
    level.setPosition(bounds.left, bounds.top);
    level.setFillColor(fill);
    target.draw(level);
}

sf::Color health_colour(const std::uint8_t hp) {
    if (hp > 60) return {86, 186, 72};
    if (hp > 30) return {236, 152, 40};
    return {220, 60, 52};
}

std::string padded(const std::uint32_t score) {
    std::string digits = std::to_string(score);
    if (digits.size() < kScoreDigits) digits.insert(0, kScoreDigits - digits.size(), '0');
    return digits;
}

} // namespace

Hud::Hud(const sf::Font& font, const sf::Texture& windows, const sf::Texture& tooth,
         const std::string& asset_dir)
    : font_(font), windows_(windows), tooth_(tooth) {
    sf::Image sheet;
    const std::string path = asset_dir + "/FreeGUI/png/Button.png";
    if (!sheet.loadFromFile(path)) throw std::runtime_error("Unable to load HUD tiles: " + path);
    const auto make_tile = [&sheet](sf::Texture& texture, const sf::Color& colour) {
        sf::Image tile;
        tile.create(static_cast<unsigned>(kTile.width), static_cast<unsigned>(kTile.height));
        for (unsigned y = 0; y < tile.getSize().y; ++y) {
            for (unsigned x = 0; x < tile.getSize().x; ++x) {
                const sf::Color source = sheet.getPixel(static_cast<unsigned>(kTile.left) + x,
                                                        static_cast<unsigned>(kTile.top) + y);
                sf::Color pixel = recolour(source.r, colour);
                pixel.a = source.a;
                tile.setPixel(x, y, pixel);
            }
        }
        texture.loadFromImage(tile);
        texture.setSmooth(true);
    };
    for (std::size_t slot = 0; slot < tiles_.size(); ++slot) make_tile(tiles_[slot], kPlayerColors[slot]);
    make_tile(spectator_tile_, sf::Color(150, 150, 150));
}

void Hud::draw_plate(sf::RenderTarget& target, const sf::IntRect& source, const int cap,
                     const sf::FloatRect& bounds) const {
    // Three slices: the ends keep their shape, only the middle stretches.
    const float scale = bounds.height / static_cast<float>(source.height);
    const float end = std::min(static_cast<float>(cap) * scale, bounds.width / 2.f);
    draw_region(target, windows_, {source.left, source.top, cap, source.height},
                {bounds.left, bounds.top, end, bounds.height});
    draw_region(target, windows_, {source.left + cap, source.top, source.width - 2 * cap, source.height},
                {bounds.left + end, bounds.top, bounds.width - 2.f * end, bounds.height});
    draw_region(target, windows_, {source.left + source.width - cap, source.top, cap, source.height},
                {bounds.left + bounds.width - end, bounds.top, end, bounds.height});
}

void Hud::draw_card(sf::RenderTarget& target, const protocol::LobbyMember& member, const bool you,
                    const std::optional<std::uint8_t> hp, const sf::FloatRect& bounds) const {
    const bool spectator = member.role == protocol::MemberRole::Spectator;
    const float middle = bounds.top + bounds.height / 2.f;
    const float tile = bounds.height;
    const float plate_height = std::round(bounds.height * 0.72f);
    const sf::FloatRect plate{std::round(bounds.left + tile * 0.55f), std::round(middle - plate_height / 2.f),
                              bounds.width - tile * 0.55f, plate_height};
    draw_plate(target, kPlate, kPlateCap, plate);

    // The tile sits over the plate's left end, the tooth inside it.
    const float tile_height = tile * static_cast<float>(kTile.height) / static_cast<float>(kTile.width);
    draw_region(target, spectator ? spectator_tile_ : tiles_[member.slot - 1U], {0, 0, kTile.width, kTile.height},
                {bounds.left, middle - tile_height / 2.f, tile, tile_height});
    const float portrait_width = tile * 0.78f;
    const float portrait_height = portrait_width * static_cast<float>(kPortrait.height) /
                                  static_cast<float>(kPortrait.width);
    draw_region(target, tooth_, kPortrait,
                {bounds.left + (tile - portrait_width) / 2.f, middle - portrait_height / 2.f + tile * 0.02f,
                 portrait_width, portrait_height});

    const float text_size = plate_height * 0.42f;
    const float inner_left = bounds.left + tile + plate_height * 0.18f;
    const float inner_right = plate.left + plate.width - plate_height * 0.32f;
    // With a health bar the text moves up a little to make room for it below.
    const float text_middle = plate.top + plate.height * (hp ? 0.42f : 0.47f); // the frame is thicker below
    if (hp) {
        const float bar_height = std::max(2.f, std::round(plate.height * 0.1f));
        draw_bar(target, {inner_left, std::round(plate.top + plate.height * 0.7f), inner_right - inner_left, bar_height},
                 static_cast<float>(*hp) / 100.f, health_colour(*hp));
    }

    // Score on the right: star then digits; a spectator has no score in this game yet.
    sf::Text score = make_text(font_, spectator ? "spectateur" : padded(member.score),
                               spectator ? text_size * 0.85f : text_size, spectator ? kMuted : kInk);
    if (spectator) score.setStyle(sf::Text::Italic);
    const float score_left = inner_right - score.getLocalBounds().width - score.getLocalBounds().left;
    place(score, font_, score_left, text_middle);
    target.draw(score);
    float star_left = score_left;
    if (!spectator) {
        const float star = plate_height * 0.62f;
        star_left = score_left - star - plate_height * 0.1f;
        draw_region(target, windows_, kStar,
                    {star_left, text_middle - star / 2.f, star,
                     star * static_cast<float>(kStar.height) / static_cast<float>(kStar.width)});
    }

    // Name on the left in the player's colour, shortened to fit; "(toi)" marks our own card.
    sf::Text mark = make_text(font_, "(toi)", text_size * 0.8f, kInk);
    mark.setStyle(sf::Text::Regular);
    const float mark_width = you ? mark.getLocalBounds().width + text_size * 0.35f : 0.f;
    const float room = star_left - plate_height * 0.15f - inner_left - mark_width;
    std::string label = member.name;
    sf::Text name = make_text(font_, label, text_size, text_colour(player_color(member.slot)));
    while (name.getLocalBounds().width > room && label.size() > 1) {
        label.pop_back();
        name.setString(label + "..");
    }
    place(name, font_, inner_left, text_middle);
    target.draw(name);
    if (you) {
        place(mark, font_, inner_left + name.getLocalBounds().width + text_size * 0.35f, text_middle);
        target.draw(mark);
    }
}

void Hud::draw_boss(sf::RenderTarget& target, const Boss& boss, const sf::FloatRect& play) const {
    const float unit = play.height / static_cast<float>(config::kHeight);
    const float height = std::round(kRowHeight * unit * 0.85f);
    const float width = std::round(play.width * 0.42f);
    const sf::FloatRect plate{std::round(play.left + (play.width - width) / 2.f),
                              std::round(play.top + play.height - height - 10.f * unit), width, height};
    draw_plate(target, kPlate, kPlateCap, plate);
    std::string name = boss.name;
    if (!name.empty()) name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    sf::Text label = make_text(font_, name, height * 0.36f, kInk);
    const float middle = plate.top + height * 0.47f;
    const float left = plate.left + height * 0.45f;
    place(label, font_, left, middle);
    target.draw(label);
    const float bar_left = left + label.getLocalBounds().width + height * 0.35f;
    const float bar_height = std::round(height * 0.26f);
    draw_bar(target, {bar_left, std::round(middle - bar_height / 2.f), plate.left + width - height * 0.45f - bar_left,
                      bar_height},
             static_cast<float>(boss.hp) / 100.f, sf::Color(214, 52, 64));
}

void Hud::draw(sf::RenderTarget& target, const protocol::LobbyState& lobby,
               const std::vector<protocol::Entity>& entities, const std::optional<Boss>& boss,
               const sf::FloatRect& play, const float menu_left) const {
    const sf::View previous = target.getView();
    const sf::Vector2f size(target.getSize());
    target.setView(sf::View(sf::FloatRect(0.f, 0.f, size.x, size.y)));

    const float unit = play.height / static_cast<float>(config::kHeight); // pixels per game unit
    const float bar = play.top;                                           // top and bottom bars are equal
    const float wanted = kRowHeight * unit;
    const bool in_bars = bar >= wanted * 0.85f;

    // Cards: one column per slot, so a player keeps their place.
    sf::FloatRect row;
    if (in_bars) {
        const float height = std::min(bar * 0.82f, wanted * 1.5f);
        row = {play.left, std::round((bar - height) / 2.f), play.width, height};
    } else {
        const float margin = 8.f * unit;
        row = {play.left + margin, play.top + margin, menu_left - play.left - 2.f * margin, wanted};
    }
    const float column = row.width / static_cast<float>(config::kMaxPlayers);
    const float gap = row.height * 0.35f;
    const float card_width = std::min(column - gap, row.height * 7.f);
    for (const auto& member : lobby.members) {
        if (member.slot < 1 || member.slot > config::kMaxPlayers) continue;
        const float left = row.left + column * static_cast<float>(member.slot - 1U) + (column - card_width) / 2.f;
        // A player with a ship shows its health; a spectator has none.
        std::optional<std::uint8_t> hp;
        for (const auto& entity : entities)
            if (entity.kind == config::kPlayerKind && entity.id == member.slot) hp = entity.hp;
        draw_card(target, member, member.slot == lobby.yourSlot, hp, {std::round(left), row.top, card_width, row.height});
    }

    // Bottom bar: the lobby's name and room code.
    if (bar >= wanted * 0.6f) {
        const float height = std::round(std::min(bar * 0.62f, wanted * 0.9f));
        const sf::String label = sf::String::fromUtf8(lobby.name.begin(), lobby.name.end()) +
                                 sf::String(L"   ·   Code ") +
                                 sf::String::fromUtf8(lobby.code.begin(), lobby.code.end());
        sf::Text info = make_text(font_, label, height * 0.46f, kInk);
        const float width = info.getLocalBounds().width + height * 1.6f;
        const sf::FloatRect plate{std::round(play.left + (play.width - width) / 2.f),
                                  std::round(play.top + play.height + (bar - height) / 2.f), width, height};
        draw_plate(target, kInfoPlate, kInfoCap, plate);
        place(info, font_, plate.left + height * 0.8f - info.getLocalBounds().left, plate.top + height / 2.f);
        target.draw(info);
    }
    if (boss) draw_boss(target, *boss, play);
    target.setView(previous);
}

} // namespace ntype::client
