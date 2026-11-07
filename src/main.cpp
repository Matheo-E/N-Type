#include "../engine/include/System.hpp"
#include "../engine/include/Factory.hpp"
#include "../engine/include/Loader.hpp"

#include <string>

int main(void) {
    World world;
    if (!LoadEnemies(world, std::string(NTYPE_ASSETS_DIR) + "/data/enemies.json")) {
        return 1;
    }
    sf::RenderWindow win;
    CreateGame(win, "Ntype");
    if (!LoadTextures(world, NTYPE_ASSETS_DIR)) {
        return 1;
    }
    int player = CreateEPlayer(world, {50.0f, 500.0f});
    CreateEEnemy(world, {250.0f, 100.0f}, 1);  // chomper
    CreateEEnemy(world, {550.0f, 100.0f}, 4);  // theice (boss)
    sf::Clock clock;
    float dt = clock.restart().asSeconds();
    while(win.isOpen()) {
        Action(world, win, dt, player);
        dt = clock.restart().asSeconds();
    }
}
