// Engine sandbox: the engine alone, in one window, with no network. Not part of the game.
//
// A showcase of what the engine can do. The buttons on the left (or the keys 1 to 9) spawn:
//   1 Burst orbs       a slow orb that bursts into a ring of shards
//   2 Spinning rings   rings of slow bullets turning clockwise, then counterclockwise...
//   3 Spiral           a two-armed spiral stream
//   4 Homing shots     shots that keep steering toward you
//   5 Loop squadron    six fighters curving in, looping, and leaving
//   6 Centipede        segments following a leader; killed segments drop rapid_fire
//   7 Item rain        rapid_fire items drifting with the background
//   8 Auto centipede   a new centipede whenever the last one is gone (on/off)
//   9 Clear all
// Move with Z Q S D, shoot with Space. Everything here is built from code, so the game's data
// files stay untouched: enemy types and patterns borrow the game's sprites.

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "Factory.hpp"
#include "Loader.hpp"
#include "Math.hpp"
#include "System.hpp"
#include "UiFont.hpp"

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

// ---- The showcase ------------------------------------------------------------

// Enemy types, attack and flight patterns made here (ids clear of the game's data).
constexpr int BursterType = 101;
constexpr int SpinnerType = 102;
constexpr int SpiralType = 103;
constexpr int HomerType = 104;
constexpr int LooperType = 105;
constexpr int OrbPattern = 201;
constexpr int ShardPattern = 202;
constexpr int RingPattern = 203;
constexpr int SpiralPattern = 204;
constexpr int HomingPattern = 205;
constexpr int LoopPath = 101;
constexpr int ChaseFlight = 1; // flight_patterns.json: chase_player

// An attack pattern with the look of one of the game's, at another size.
AttackPattern Borrow(const World& world, int look, float scale) {
    AttackPattern pattern = world.AttackPatterns.at(look);
    pattern.Count = 1;
    pattern.SpreadDegrees = 0;
    pattern.Aimed = false;
    pattern.FlightPatternId = -1;
    pattern.AnimationFrames.clear();
    pattern.Scale = scale;
    return pattern;
}

// An enemy type with the look of one of the game's.
EnemyData Enemy(const World& world, int look, int hp, int attack, float interval) {
    EnemyData enemy = world.EnemyTypes.at(look);
    enemy.Hp = hp;
    enemy.Speed = 0;
    enemy.AttackPatternId = attack;
    enemy.FireInterval = interval; // 0: the sandbox fires it itself
    enemy.FlightPatternId = -1;
    enemy.ContactDamage = 1;
    return enemy;
}

void AddShowcase(World& world) {
    AttackPattern orb = Borrow(world, 3, 0.03f); // spiked ice ball
    orb.Aimed = true;
    orb.Speed = 90;
    orb.Radius = 14;
    orb.Lifetime = 10;
    world.AttackPatterns[OrbPattern] = orb;
    AttackPattern shard = Borrow(world, 3, 0.012f);
    shard.Speed = 170;
    shard.Radius = 6;
    shard.Lifetime = 6;
    world.AttackPatterns[ShardPattern] = shard;
    AttackPattern ring = Borrow(world, 4, 0.012f); // candy blob
    ring.Speed = 70;
    ring.Radius = 7;
    ring.Lifetime = 12;
    world.AttackPatterns[RingPattern] = ring;
    AttackPattern spiral = Borrow(world, 9, 0.03f); // infected tooth shot
    spiral.Speed = 150;
    spiral.Radius = 6;
    spiral.Lifetime = 9;
    world.AttackPatterns[SpiralPattern] = spiral;
    AttackPattern homing = Borrow(world, 7, 0.018f); // toxic blob
    homing.Aimed = true;
    homing.Speed = 140;
    homing.Radius = 7;
    homing.Lifetime = 5;
    homing.FlightPatternId = ChaseFlight; // the engine's chase flight, on a bullet
    world.AttackPatterns[HomingPattern] = homing;

    world.EnemyTypes[BursterType] = Enemy(world, 2, 20, OrbPattern, 2.0f);     // smice
    world.EnemyTypes[SpinnerType] = Enemy(world, 6, 25, RingPattern, 0.0f);    // jellybean
    world.EnemyTypes[SpiralType] = Enemy(world, 10, 30, SpiralPattern, 0.0f);  // evil mask
    world.EnemyTypes[HomerType] = Enemy(world, 9, 15, HomingPattern, 1.4f);    // syringe
    EnemyData looper = Enemy(world, 1, 4, 1, 1.8f);                            // chomper, ice arrows
    looper.FlightPatternId = LoopPath;
    world.EnemyTypes[LooperType] = looper;

    // Curve in from the top right, loop once in the middle, leave to the left.
    PathStep in;
    in.Kind = StepKind::Curve;
    in.Control1 = {-300.0f, 0.0f};
    in.Control2 = {-600.0f, 60.0f};
    in.To = {-650.0f, 200.0f};
    in.Duration = 2.0f;
    PathStep loop;
    loop.Kind = StepKind::Arc;
    loop.Radius = 90.0f;
    loop.SweepDegrees = 360.0f;
    loop.Duration = 2.0f;
    PathStep out;
    out.Kind = StepKind::Move;
    out.To = {-1500.0f, 200.0f};
    out.Duration = 2.5f;
    FlightPattern path;
    path.Name = "loop_squadron";
    path.Type = FlightType::Path;
    path.Steps = {in, loop, out};
    world.FlightPatterns[LoopPath] = path;
}

// One bullet of `shooter`'s attack pattern, from `from`, along `degrees` (0 = down).
int Shoot(World& world, int shooter, sf::Vector2f from, float degrees) {
    const int bullet = CreateEBullet(world, shooter);
    if (bullet >= 0) {
        world.Positions[bullet].Xy = from;
        world.Velocities[bullet].Xy = Direction(degrees) * world.AttackPatterns.at(world.Spawners.at(shooter).PatternId).Speed;
    }
    return bullet;
}

// A bullet of a ring that spins around where it was fired while it spreads out.
class Spin {
public:
    sf::Vector2f Centre;
    float Angle = 0;  // degrees, at the start
    float Turn = 0;   // degrees per second: positive turns counterclockwise on screen
    float Age = 0;
};

// What the showcase's enemies do beyond the engine's own systems.
class Showcase {
public:
    std::map<int, float> Orbs;          // orb -> seconds since fired; it bursts at BurstAge
    std::map<int, float> Spinners;      // enemy -> seconds since its last ring
    std::map<int, int> Rings;           // enemy -> rings fired (their turn alternates)
    std::map<int, Spin> Spinning;       // ring bullet -> its motion
    std::map<int, float> Spirals;       // enemy -> seconds since its last shot
    std::map<int, float> SpiralAngle;   // enemy -> angle of its next shot
    std::vector<float> Squadron;        // seconds before each pending looper enters
    std::vector<int> SquadronY;         // and its height
    int Placed = 0;                     // spreads spawned enemies over the height

    static constexpr float BurstAge = 1.4f;
    static constexpr int Shards = 12;
    static constexpr float RingEvery = 1.6f;
    static constexpr int RingBullets = 14;
    static constexpr float RingTurn = 45.0f;
    static constexpr float SpiralEvery = 0.07f;
    static constexpr float SpiralStep = 11.0f;

    float NextY() { return 140.0f + static_cast<float>(Placed++ % 4) * 150.0f; }

    // Before Movement: new shots, bursts and the spinning bullets' velocities for this frame.
    void Update(World& world, float dt) {
        Forget(world);
        TrackOrbs(world);
        Burst(world, dt);
        FireRings(world, dt);
        FireSpirals(world, dt);
        Turn(world, dt);
        EnterSquadron(world, dt);
    }

    void Clear() { *this = Showcase(); }

private:
    void Forget(World& world) {
        const auto gone = [&world](const auto& entry) { return world.Positions.count(entry.first) == 0; };
        std::erase_if(Orbs, gone);
        std::erase_if(Spinners, gone);
        std::erase_if(Rings, gone);
        std::erase_if(Spinning, gone);
        std::erase_if(Spirals, gone);
        std::erase_if(SpiralAngle, gone);
    }

    void TrackOrbs(World& world) {
        for (const auto& [id, bullet] : world.Bullets) {
            if (bullet.PatternId == OrbPattern && Orbs.count(id) == 0) {
                Orbs[id] = 0.0f;
            }
        }
    }

    // An orb bursts into a ring of shards. The shards come from a short-lived emitter at the
    // orb's place: bullets are always fired by an entity with an attack pattern.
    void Burst(World& world, float dt) {
        std::vector<int> bursting;
        for (auto& [id, age] : Orbs) {
            age += dt;
            if (age >= BurstAge) {
                bursting.push_back(id);
            }
        }
        for (int orb : bursting) {
            const int emitter = CreateE(world);
            world.Positions[emitter].Xy = world.Positions.at(orb).Xy;
            world.Spawners[emitter].PatternId = ShardPattern; // no interval: never fires by itself
            for (int i = 0; i < Shards; ++i) {
                Shoot(world, emitter, world.Positions.at(emitter).Xy, 360.0f / Shards * static_cast<float>(i));
            }
            world.Destroy(emitter);
            world.Destroy(orb);
            Orbs.erase(orb);
        }
    }

    void FireRings(World& world, float dt) {
        for (auto& [id, timer] : Spinners) {
            timer += dt;
            if (timer < RingEvery) {
                continue;
            }
            timer -= RingEvery;
            const float turn = Rings[id]++ % 2 == 0 ? -RingTurn : RingTurn; // clockwise, then back
            const float offset = static_cast<float>(Rings[id]) * 7.0f;
            for (int i = 0; i < RingBullets; ++i) {
                const float angle = offset + 360.0f / RingBullets * static_cast<float>(i);
                const int bullet = Shoot(world, id, world.Positions.at(id).Xy, angle);
                if (bullet >= 0) {
                    Spinning[bullet] = {world.Positions.at(id).Xy, angle, turn, 0.0f};
                }
            }
        }
    }

    void FireSpirals(World& world, float dt) {
        for (auto& [id, timer] : Spirals) {
            timer += dt;
            while (timer >= SpiralEvery) {
                timer -= SpiralEvery;
                float& angle = SpiralAngle[id];
                for (float arm : {0.0f, 180.0f}) {
                    Shoot(world, id, world.Positions.at(id).Xy, angle + arm);
                }
                angle += SpiralStep;
            }
        }
    }

    // Each ring bullet goes where its spinning ring puts it this frame: out from the centre at
    // the pattern's speed, turned by the ring's turn.
    void Turn(World& world, float dt) {
        if (dt <= 0.0f) {
            return;
        }
        const float speed = world.AttackPatterns.at(RingPattern).Speed;
        for (auto& [id, spin] : Spinning) {
            spin.Age += dt;
            const sf::Vector2f target = spin.Centre + Direction(spin.Angle + spin.Turn * spin.Age) * (speed * spin.Age);
            world.Velocities[id].Xy = (target - world.Positions.at(id).Xy) / dt;
        }
    }

    void EnterSquadron(World& world, float dt) {
        for (std::size_t i = 0; i < Squadron.size();) {
            Squadron[i] -= dt;
            if (Squadron[i] > 0.0f) {
                ++i;
                continue;
            }
            CreateEEnemy(world, {1300.0f, static_cast<float>(SquadronY[i])}, LooperType);
            Squadron.erase(Squadron.begin() + static_cast<long>(i));
            SquadronY.erase(SquadronY.begin() + static_cast<long>(i));
        }
    }
};

// ---- The buttons ----------------------------------------------------------------

class Button {
public:
    std::string Label;
    std::function<void()> Press;
};

const sf::Vector2f PanelAt = {12.0f, 12.0f};
constexpr float ButtonWidth = 210.0f;
constexpr float ButtonHeight = 30.0f;
constexpr float ButtonGap = 4.0f;

sf::FloatRect ButtonBox(std::size_t i) {
    return {PanelAt.x, PanelAt.y + static_cast<float>(i) * (ButtonHeight + ButtonGap), ButtonWidth, ButtonHeight};
}

void DrawButtons(sf::RenderWindow& window, const std::vector<Button>& buttons, const sf::Font* font) {
    const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        const sf::FloatRect box = ButtonBox(i);
        sf::RectangleShape back({box.width, box.height});
        back.setPosition(box.left, box.top);
        back.setFillColor(box.contains(mouse) ? sf::Color(70, 110, 170, 230) : sf::Color(25, 35, 60, 200));
        back.setOutlineColor(sf::Color(140, 180, 240, 220));
        back.setOutlineThickness(1.0f);
        window.draw(back);
        if (font != nullptr) {
            sf::Text text(std::to_string(i + 1) + "  " + buttons[i].Label, *font, 15);
            text.setPosition(box.left + 10.0f, box.top + 5.0f);
            text.setFillColor(sf::Color::White);
            window.draw(text);
        }
    }
}

// Presses the button under a click; true if there was one.
bool Click(const std::vector<Button>& buttons, sf::Vector2f at) {
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (ButtonBox(i).contains(at)) {
            buttons[i].Press();
            return true;
        }
    }
    return false;
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
    AddShowcase(world);
    sf::Font font;
    const bool labels = ntype::client::load_ui_font(font); // without a font: keys 1 to 9 still work

    int player = SpawnPlayer(world);
    Wave wave = SpawnWave(world);
    int waves = 1;
    bool autowaves = true;
    Showcase showcase;

    std::vector<Button> buttons = {
        {"Burst orbs", [&] { const int id = CreateEEnemy(world, {1100.0f, showcase.NextY()}, BursterType); (void)id; }},
        {"Spinning rings", [&] { const int id = CreateEEnemy(world, {1000.0f, showcase.NextY()}, SpinnerType); if (id >= 0) showcase.Spinners[id] = Showcase::RingEvery; }},
        {"Spiral", [&] { const int id = CreateEEnemy(world, {950.0f, showcase.NextY()}, SpiralType); if (id >= 0) showcase.Spirals[id] = 0.0f; }},
        {"Homing shots", [&] { CreateEEnemy(world, {1150.0f, showcase.NextY()}, HomerType); }},
        {"Loop squadron", [&] { for (int i = 0; i < 6; ++i) { showcase.Squadron.push_back(0.35f * static_cast<float>(i)); showcase.SquadronY.push_back(140); } }},
        {"Centipede", [&] { if (wave.Segments.empty() || !UpdateWave(world, wave)) { world.Destroy(wave.Leader); wave = SpawnWave(world); ++waves; } }},
        {"Item rain", [&] { for (int i = 0; i < 6; ++i) CreateEItem(world, {1300.0f, 120.0f + 95.0f * static_cast<float>(i)}, RapidFire); }},
        {"Auto centipede: on", nullptr},
        {"Clear all", [&] {
             std::vector<int> gone;
             for (const auto& [id, enemy] : world.Enemies) gone.push_back(id);
             for (const auto& [id, bullet] : world.Bullets) gone.push_back(id);
             for (const auto& [id, item] : world.Pickups) gone.push_back(id);
             gone.push_back(wave.Leader);
             for (int id : gone) world.Destroy(id);
             wave = Wave();
             showcase.Clear();
         }},
    };
    buttons[7].Press = [&] {
        autowaves = !autowaves;
        buttons[7].Label = std::string("Auto centipede: ") + (autowaves ? "on" : "off");
    };

    sf::Clock clock;
    float titletimer = 0.0f;
    while (window.isOpen()) {
        // The engine's frame (Action in System.cpp), with the showcase and the buttons in it.
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                Click(buttons, window.mapPixelToCoords({event.mouseButton.x, event.mouseButton.y}));
            } else if (event.type == sf::Event::KeyPressed && event.key.code >= sf::Keyboard::Num1 &&
                       event.key.code <= sf::Keyboard::Num9) {
                const auto index = static_cast<std::size_t>(event.key.code - sf::Keyboard::Num1);
                if (index < buttons.size()) {
                    buttons[index].Press();
                }
            }
        }
        const float dt = std::min(clock.restart().asSeconds(), 0.05f);
        window.clear(sf::Color::Black);
        world.Actions.Update(world.Keys);
        ReadKeyboard(world, player);
        MoveInput(world);
        Spawner(world, dt);
        Flight(world, dt);
        Follow(world, dt);
        showcase.Update(world, dt);
        Movement(world, dt);
        Collision(world);
        Pickup(world);
        Timers(world, dt);
        Cull(world);
        Animate(world, dt);
        Draw(world, window);
        DrawButtons(window, buttons, labels ? &font : nullptr);
        window.display();

        world.Deaths.clear(); // records for a game to react to: the sandbox does not need them
        if (!UpdateWave(world, wave)) {
            world.Destroy(wave.Leader);
            wave = autowaves ? SpawnWave(world) : Wave();
            waves += autowaves ? 1 : 0;
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
