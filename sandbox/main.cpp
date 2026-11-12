// Engine sandbox: the engine alone, in one window, with no network. Not part of the game.
//
// A centipede comes in from the right, stops near the middle, shoots once and leaves.
// Every segment you kill drops a rapid_fire item. When the centipede is gone a new one
// comes, until the window is closed. Move with Z Q S D, shoot with Space.

#include <algorithm>
#include <string>
#include <vector>

#include "Factory.hpp"
#include "Loader.hpp"
#include "System.hpp"

namespace {

// The centipede is made here, from code, so the game's data files stay untouched.
constexpr int CentipedeType = 100;  // enemy type id
constexpr int LeaderPath = 100;     // flight pattern id
constexpr int SegmentCount = 8;
constexpr int BorrowedLook = 7;     // gummyworm (enemies.json): its sprite is reused
constexpr int RapidFire = 1;        // items.json
const sf::Vector2f Spawn = {1300.0f, 360.0f};

void AddCentipede(World& world) {
    EnemyData segment = world.EnemyTypes.at(BorrowedLook);
    segment.Name = "centipede";
    segment.Hp = 3;
    segment.Radius = 22;
    segment.Scale = 0.06f;
    segment.Speed = 360;            // faster than the leader, so the chain keeps up
    segment.ContactDamage = 1;
    segment.AttackPatternId = 1;    // smice's aimed ice arrow
    segment.FireInterval = 4.6f;    // fires once, while stopped; gone before the next shot
    segment.FlightPatternId = -1;   // segments follow, they do not fly a pattern
    world.EnemyTypes[CentipedeType] = segment;

    // An invisible leader flies this; the segments follow it in a chain.
    PathStep in;
    in.Kind = StepKind::Move;
    in.To = {-660.0f, 0.0f};        // to the middle of the 1280 wide screen
    in.Duration = 3.0f;
    PathStep stop;
    stop.Kind = StepKind::Wait;
    stop.Duration = 2.5f;
    PathStep out;
    out.Kind = StepKind::Move;
    out.To = {-1500.0f, 0.0f};      // off the left edge
    out.Duration = 2.5f;
    FlightPattern path;
    path.Name = "stop_shoot_leave";
    path.Type = FlightType::Path;
    path.Steps = {in, stop, out};
    world.FlightPatterns[LeaderPath] = path;
}

// The engine's player is set up for an old sprite sheet: show the tooth instead, and give it
// plenty of health so the test does not end. Its shots go right, with the ice arrow's look.
int SpawnPlayer(World& world) {
    const int player = CreateEPlayer(world, {150.0f, 360.0f});
    ESprite& look = world.Sprites[player];
    look.Source = {0, 0, 724, 724};
    look.Scale = {88.0f / 724.0f, 88.0f / 724.0f};
    world.Animations[player].Frames = {0, 1, 2, 1};
    world.Animations[player].FrameTime = 0.125f;
    world.Healths[player].Hp = 99;
    world.Healths[player].MaxHp = 99;
    AttackPattern& shot = world.AttackPatterns.at(0);
    const AttackPattern& arrow = world.AttackPatterns.at(1);
    shot.TextureId = arrow.TextureId;
    shot.Source = arrow.Source;
    shot.Scale = arrow.Scale;
    shot.FlipX = false;
    return player;
}

class Wave {
public:
    int Leader = -1;
    std::vector<int> Segments;          // -1 once a segment is gone
    std::vector<sf::Vector2f> LastSeen;
};

Wave SpawnWave(World& world) {
    Wave wave;
    wave.Leader = CreateE(world);
    world.Positions[wave.Leader].Xy = Spawn;
    world.Velocities[wave.Leader];
    world.Flights[wave.Leader].PatternId = LeaderPath;
    world.Flights[wave.Leader].Origin = Spawn;
    int parent = wave.Leader;
    for (int i = 0; i < SegmentCount; ++i) {
        parent = CreateEEnemyFollower(world, Spawn, CentipedeType, parent);
        wave.Segments.push_back(parent);
        wave.LastSeen.push_back(Spawn);
    }
    return wave;
}

bool OnScreen(sf::Vector2f p) {
    return p.x > 0.0f && p.x < WindowWidth && p.y > 0.0f && p.y < WindowHeight;
}

// Keeps the chain together and drops items. A segment that disappears on screen was killed:
// it drops rapid_fire; one that disappears off screen just left. The others re-attach to the
// nearest segment still in front of them. Returns false once every segment is gone.
bool UpdateWave(World& world, Wave& wave) {
    int parent = wave.Leader;
    bool alive = false;
    for (std::size_t i = 0; i < wave.Segments.size(); ++i) {
        const int id = wave.Segments[i];
        if (id < 0) {
            continue;
        }
        auto pos = world.Positions.find(id);
        if (pos == world.Positions.end()) {
            if (OnScreen(wave.LastSeen[i])) {
                CreateEItem(world, wave.LastSeen[i], RapidFire);
            }
            wave.Segments[i] = -1;
            continue;
        }
        wave.LastSeen[i] = pos->second.Xy;
        world.Parents[id].ParentId = parent;
        parent = id;
        alive = true;
    }
    return alive;
}

void KeepInside(World& world, int player) {
    auto pos = world.Positions.find(player);
    if (pos != world.Positions.end()) {
        pos->second.Xy.x = std::clamp(pos->second.Xy.x, 0.0f, static_cast<float>(WindowWidth));
        pos->second.Xy.y = std::clamp(pos->second.Xy.y, 0.0f, static_cast<float>(WindowHeight));
    }
}

std::string Title(World& world, int player, int waves) {
    const EHealth& health = world.Healths[player];
    const float interval = world.Spawners[player].Interval;
    return "N-Type sandbox | HP " + std::to_string(health.Hp) + "/" + std::to_string(health.MaxHp) +
           " | " + std::to_string(static_cast<int>(1.0f / interval + 0.5f)) + " shots/s | wave " +
           std::to_string(waves);
}

}  // namespace

int main() {
    const std::string assets = NTYPE_ASSETS_DIR;
    World world;
    if (!LoadAttackPatterns(world, assets + "/data/attack_patterns.json") ||
        !LoadFlightPatterns(world, assets + "/data/flight_patterns.json") ||
        !LoadEnemies(world, assets + "/data/enemies.json") ||
        !LoadItems(world, assets + "/data/items.json")) {
        return 1;
    }
    sf::RenderWindow window;
    CreateGame(window, "N-Type sandbox");
    if (!LoadTextures(world, assets) || !CheckReferences(world)) {
        return 1;
    }
    AddCentipede(world);
    int player = SpawnPlayer(world);
    Wave wave = SpawnWave(world);
    int waves = 1;

    sf::Clock clock;
    float titletimer = 0.0f;
    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        Action(world, window, dt, player);
        world.Deaths.clear();    // records for a game to react to: the sandbox does not need them
        if (!UpdateWave(world, wave)) {
            world.Destroy(wave.Leader);
            wave = SpawnWave(world);
            ++waves;
        }
        if (world.Players.count(player) == 0) {
            player = SpawnPlayer(world);
        }
        KeepInside(world, player);
        titletimer += dt;
        if (titletimer > 0.25f) {
            titletimer = 0.0f;
            window.setTitle(Title(world, player, waves));
        }
    }
}
