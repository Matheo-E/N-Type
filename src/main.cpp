#include "../engine/include/System.hpp"
#include "../engine/include/Factory.hpp"
#include "../engine/include/Loader.hpp"

#include <string>

int main(void) {
    const std::string assets = NTYPE_ASSETS_DIR;
    World world;
    if (!LoadAttackPatterns(world, assets + "/data/attack_patterns.json") ||
        !LoadFlightPatterns(world, assets + "/data/flight_patterns.json") ||
        !LoadEnemies(world, assets + "/data/enemies.json")) {
        return 1;
    }
    sf::RenderWindow win;
    CreateGame(win, "Ntype");
    if (!LoadTextures(world, assets) || !CheckReferences(world)) {
        return 1;
    }
    int player = CreateEPlayer(world, {50.0f, 500.0f});
    CreateEEnemy(world, {250.0f, 100.0f}, 1);  // chomper
    CreateEEnemy(world, {550.0f, 100.0f}, 4);  // theice (boss)
    CreateEEnemy(world, {750.0f, 200.0f}, 2);  // smice: drifts left, aims at you
    CreateEEnemy(world, {700.0f, 400.0f}, 12); // carry: patrols up and down
    CreateEEnemy(world, {400.0f, -40.0f}, 13); // looper: arcs in from the top, loops, leaves
    sf::Clock clock;
    float dt = clock.restart().asSeconds();
    while(win.isOpen()) {
        Action(world, win, dt, player);
        if (world.Players.count(player) == 0) {  // the player was destroyed: game over
            win.close();
        }
        dt = clock.restart().asSeconds();
    }
}
