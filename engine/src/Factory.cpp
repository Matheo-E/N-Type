#include "../include/Factory.hpp"

namespace {

// Circles are positioned by their centre, so the position matches the hitbox.
void AddCircle(World& world, int id, float radius, sf::Color color) {
    sf::CircleShape& circle = world.Circles[id].Circle;
    circle.setRadius(radius);
    circle.setOrigin(radius, radius);
    circle.setFillColor(color);
}

void AddRectangle(World& world, int id, float length, float height, sf::Color color) {
    sf::RectangleShape& rectangle = world.Rectangles[id].Rect;
    rectangle.setSize({length, height});
    rectangle.setFillColor(color);
}

void AddHurtbox(World& world, int id, float radius, sf::Vector2f offset) {
    world.Hurtboxes[id].Radius = radius;
    world.Hurtboxes[id].Offset = offset;
}

void AddHitbox(World& world, int id, float radius) {
    world.Hitboxes[id].Radius = radius;
}

void SetLayer(World& world, int id, int layer, int mask) {
    world.Layers[id].Layer = layer;
    world.Layers[id].Mask = mask;
}

// An enemy only gets a Spawner / Flight component if its data asks for one.
void AddEnemyPatterns(World& world, int id, const EnemyData& data, sf::Vector2f pos) {
    if (data.AttackPatternId >= 0) {
        world.Spawners[id].Interval = data.FireInterval;
        world.Spawners[id].PatternId = data.AttackPatternId;
    }
    if (data.FlightPatternId >= 0) {
        world.Flights[id].PatternId = data.FlightPatternId;
        world.Flights[id].Origin = pos;
    }
}

// A sprite if the enemy type has a texture, otherwise a plain circle.
void AddEnemyLook(World& world, int id, const EnemyData& data) {
    if (data.TextureId < 0) {
        AddCircle(world, id, data.Radius, data.Color);
        return;
    }
    world.TextureIds[id].TextId = data.TextureId;
    world.TextureIds[id].Source = data.Source;
    world.TextureIds[id].Scale = {data.Scale, data.Scale};
}

}  // namespace

int CreateE(World& world) {
    int id = world.NextId++;
    return id;
}

int CreateEPlayer(World& world, sf::Vector2f pos) {
    int id = CreateE(world);
    world.Players.emplace(id, EPlayer{});
    world.Positions[id].Xy = pos;
    world.Velocities[id];  // [] creates the entry: starts at {0, 0}
    world.Speeds[id].Value = 200.0f;
    world.Healths[id].Hp = 3;
    world.Healths[id].MaxHp = 3;
    world.Spawners[id].Interval = 0.15f;  // pattern 0: fires straight up while Space is held
    world.Spawners[id].PatternId = 0;
    AddRectangle(world, id, 32.0f, 32.0f, sf::Color::Green);
    // Bullet hell: the part that can be hit is much smaller than the sprite.
    AddHurtbox(world, id, 3.0f, {16.0f, 16.0f});
    SetLayer(world, id, LayerPlayer, 0);
    return id;
}

int CreateEEnemy(World& world, sf::Vector2f pos, int enemyid) {
    auto type = world.EnemyTypes.find(enemyid);
    if (type == world.EnemyTypes.end()) {
        return -1;
    }
    const EnemyData& data = type->second;
    int id = CreateE(world);

    world.Enemies.emplace(id, EEnemy{});
    world.Names[id].Name = data.Name;
    world.Positions[id].Xy = pos;
    world.Velocities[id];
    world.Speeds[id].Value = data.Speed;
    world.Healths[id].Hp = data.Hp;
    world.Healths[id].MaxHp = data.Hp;
    AddEnemyPatterns(world, id, data, pos);
    AddEnemyLook(world, id, data);
    AddHurtbox(world, id, data.Radius, {0.0f, 0.0f});
    SetLayer(world, id, LayerEnemy, LayerPlayer);
    return id;
}

// One bullet, fired by spawnerid. It starts at the spawner and gets a default
// velocity: the pattern system overrides it with the bullet's real direction.
int CreateEBullet(World& world, int spawnerid) {
    const bool fromplayer = world.Players.count(spawnerid) > 0;
    auto spawner = world.Positions.find(spawnerid);
    int id = CreateE(world);

    world.Bullets.emplace(id, EBullet{});
    world.Positions[id].Xy = (spawner != world.Positions.end()) ? spawner->second.Xy : sf::Vector2f(0, 0);
    world.Velocities[id].Xy = fromplayer ? sf::Vector2f(0, -600) : sf::Vector2f(0, 150);
    world.OwnerIds[id].OId = spawnerid;
    world.Damages[id].Dmg = 1;
    world.Lifetimes[id].Lifetime = 10.0f;
    AddCircle(world, id, 4.0f, sf::Color::White);
    AddHitbox(world, id, 4.0f);
    SetLayer(world, id, fromplayer ? LayerPlayerBullet : LayerEnemyBullet,
             fromplayer ? LayerEnemy : LayerPlayer);
    return id;
}

// One enemy that follows parentid. The system builds a whole train by calling
// this repeatedly with the previous enemy as the parent.
int CreateEEnemyFollower(World& world, sf::Vector2f pos, int enemyid, int parentid) {
    int id = CreateEEnemy(world, pos, enemyid);
    if (id >= 0) {
        world.Parents[id].ParentId = parentid;
    }
    return id;
}

int CreateEStatic(World& world, sf::Vector2f pos) {
    int id = CreateE(world);
    world.Positions[id].Xy = pos;
    return id;
}
