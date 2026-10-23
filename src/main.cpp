#include "../engine/include/System.hpp"
#include "../engine/include/Factory.hpp"

int main(void) {
    World world;
    sf::RenderWindow win;
    Create_Game(win, "Ntype");
    int player = Create_E_Player(world, {50.0f, 20.0f});
    sf::Clock clock;
    float dt = clock.restart().asSeconds();
    while(win.isOpen()) {
        Action(world, win, dt, player);
        dt = clock.restart().asSeconds();
    }
}
