#include <SFML/Graphics.hpp>
#include "world/world.h"
#include <chrono>

int main()
{
    sf::ContextSettings windowSettings;
    windowSettings.antialiasingLevel = 8;

    constexpr int WIDTH = 1920;
    constexpr int HEIGHT = 1080;

    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Cell Battles",
                            sf::Style::Default, windowSettings);

    WorldSettings worldSettings;
    worldSettings.width = 1920;
    worldSettings.height = 1080;

    worldSettings.pixelsPerChunk = 10;

    worldSettings.numTeams = 4;
    worldSettings.cellRadius = 3;
    worldSettings.initialCellsPerTeam = 1000;
    worldSettings.cellAttackRange = 10;

    worldSettings.teamColors = {
            sf::Color::Green,
            sf::Color::Red,
            sf::Color::Yellow,
            sf::Color::Blue
    };
    worldSettings.teamSpawns = {
            {300,        300},
            {300,        1080 - 300},
            {1920 - 300, 300},
            {1920 - 300, 1080 - 300}
    };
    worldSettings.spawnRadius = 5;

    World world = World(worldSettings, 3211);

    auto lastTime = std::chrono::steady_clock::now().time_since_epoch().count();
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
            else if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::F1)
                    world.viewMode = DEFAULT;
                else if (event.key.code == sf::Keyboard::F2)
                    world.viewMode = SUPPLY;
            }
        }

        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        float delta = (float) (now - lastTime) / 1000000000.f;
        if (delta > 0.05) delta = 0.05;
        world.step(delta);
        lastTime = now;

        window.clear();
        window.draw(world);
        window.display();
    }

    return 0;
}