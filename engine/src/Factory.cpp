#include "../include/Factory.hpp"
#include "../include/Math.hpp"

namespace {

// Texture id from assets/data/textures.json.
constexpr int TexturePlayers = 1;

ESprite Look(int textureid, sf::IntRect source, float scale, bool flip = false) {
    ESprite look;
    look.FlipX = flip;
    look.TextureId = textureid;
    look.Source = source;
    look.Scale = {scale, scale};
    return look;
}

// Makes the entity visible, on the given draw layer (Z, see Component.hpp).
void AddSprite(World& world, int id, const ESprite& look, int z) {
    world.Sprites[id] = look;
    world.ZIndexes[id].Z = z;
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

// The sprite of an enemy or item type, with its animation if it has one.
template <class Data>
void AddLook(World& world, int id, const Data& data, int z) {
    AddSprite(world, id, Look(data.TextureId, data.Source, data.Scale, data.FlipX), z);
    if (!data.AnimationFrames.empty()) {
        world.Animations[id].Frames = data.AnimationFrames;
        world.Animations[id].FrameTime = data.AnimationFrameTime;
    }
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
    world.Spawners[id].Interval = 0.15f;  // its attack pattern 0 is the player's shot
    world.Spawners[id].PatternId = 0;
    AddSprite(world, id, Look(TexturePlayers, {67, 2, 32, 15}, 2.0f), ZPlayer);
    // Bullet hell: the part that can be hit is much smaller than the sprite.
    AddHurtbox(world, id, 3.0f, {0.0f, 0.0f});
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

    world.Enemies.emplace(id, EEnemy{enemyid});
    world.Names[id].Name = data.Name;
    world.Positions[id].Xy = pos;
    world.Velocities[id];
    world.Speeds[id].Value = data.Speed;
    world.Healths[id].Hp = data.Hp;
    world.Healths[id].MaxHp = data.Hp;
    AddEnemyPatterns(world, id, data, pos);
    AddLook(world, id, data, ZEnemy);
    AddHurtbox(world, id, data.Radius, {0.0f, 0.0f});
    if (data.ContactDamage > 0) {  // touching it hurts the player (its layer mask already targets the player)
        AddHitbox(world, id, data.Radius);
        world.Damages[id].Dmg = data.ContactDamage;
    }
    SetLayer(world, id, LayerEnemy, LayerPlayer);
    return id;
}

// One bullet, fired by spawnerid. Everything about the bullet (speed, damage, size,
// lifetime, look) comes from the attack pattern stored in the spawner. It starts at the
// spawner and flies along the pattern's angle: a system that fires a fan or a ring
// overrides the velocity for each bullet. Returns -1 (nothing is created) if the
// spawner has no attack pattern, or the pattern is unknown.
int CreateEBullet(World& world, int spawnerid) {
    auto spawner = world.Spawners.find(spawnerid);
    if (spawner == world.Spawners.end()) {
        return -1;
    }
    auto found = world.AttackPatterns.find(spawner->second.PatternId);
    if (found == world.AttackPatterns.end()) {
        return -1;
    }
    const AttackPattern& pattern = found->second;
    const bool fromplayer = world.Players.count(spawnerid) > 0;
    auto where = world.Positions.find(spawnerid);
    int id = CreateE(world);

    world.Bullets.emplace(id, EBullet{spawner->second.PatternId});
    world.Positions[id].Xy = where != world.Positions.end() ? where->second.Xy : sf::Vector2f(0, 0);
    world.Velocities[id].Xy = Direction(pattern.AngleDegrees) * pattern.Speed;
    world.OwnerIds[id].OId = spawnerid;
    auto stats = world.Stats.find(spawnerid);  // the shooter's Attack adds to its bullets
    world.Damages[id].Dmg = pattern.Damage + (stats != world.Stats.end() ? stats->second.Attack : 0);
    world.Lifetimes[id].Lifetime = pattern.Lifetime;
    if (pattern.FlightPatternId >= 0) {  // the bullet follows a flight pattern, turned to its own angle
        world.Speeds[id].Value = pattern.Speed;
        world.Flights[id].PatternId = pattern.FlightPatternId;
        world.Flights[id].Origin = world.Positions[id].Xy;
        world.Flights[id].Heading = pattern.AngleDegrees;
    }
    if (pattern.TextureId >= 0) {
        AddLook(world, id, pattern, ZBullet);
    }
    AddHitbox(world, id, pattern.Radius);
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

int CreateEItem(World& world, sf::Vector2f pos, int itemid) {
    auto type = world.ItemTypes.find(itemid);
    if (type == world.ItemTypes.end()) {
        return -1;
    }
    const ItemData& data = type->second;
    int id = CreateE(world);

    world.Pickups[id].ItemId = itemid;
    world.Names[id].Name = data.Name;
    world.Positions[id].Xy = pos;
    world.Velocities[id];
    world.Speeds[id].Value = data.Speed;
    // A hurtbox to be touched, but no collision layer: bullets ignore it and it hurts nobody.
    AddHurtbox(world, id, data.Radius, {0.0f, 0.0f});
    AddLook(world, id, data, ZEnemy);
    if (data.FlightPatternId >= 0) {
        world.Flights[id].PatternId = data.FlightPatternId;
        world.Flights[id].Origin = pos;
    }
    if (data.Lifetime > 0.0f) {
        world.Lifetimes[id].Lifetime = data.Lifetime;
    }
    return id;
}

bool AddEnemyLook(World& world, int id, int enemyid) {
    auto type = world.EnemyTypes.find(enemyid);
    if (type == world.EnemyTypes.end()) {
        return false;
    }
    AddLook(world, id, type->second, ZEnemy);
    return true;
}

bool AddShotLook(World& world, int id, int patternid) {
    auto pattern = world.AttackPatterns.find(patternid);
    if (pattern == world.AttackPatterns.end() || pattern->second.TextureId < 0) {
        return false;
    }
    AddLook(world, id, pattern->second, ZBullet);
    return true;
}

bool AddItemLook(World& world, int id, int itemid) {
    auto type = world.ItemTypes.find(itemid);
    if (type == world.ItemTypes.end()) {
        return false;
    }
    AddLook(world, id, type->second, ZEnemy);
    return true;
}
