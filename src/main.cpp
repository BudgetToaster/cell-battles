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
    window.setFramerateLimit(0);
    window.setVerticalSyncEnabled(false);

    WorldSettings worldSettings;
    worldSettings.width = WIDTH;
    worldSettings.height = HEIGHT;

    worldSettings.pixelsPerChunk = 10;

    worldSettings.numTeams = 4;
    worldSettings.cellRadius = 3;
    worldSettings.initialCellsPerTeam = 10;
    worldSettings.cellAttackRange = 10;
    worldSettings.supplyDiffusionRate = 1.f;

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

    sf::Font robotoFont;
    robotoFont.loadFromFile("roboto/Roboto-Light.ttf");

    sf::Text statsText;
    statsText.setFont(robotoFont);
    statsText.setCharacterSize(11);
    statsText.setFillColor(sf::Color::White);

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
                else if(event.key.code == sf::Keyboard::F3)
                    world.viewMode = SUPPLY_GENERATION;
                else if(event.key.code == sf::Keyboard::Escape)
                    std::exit(0);
            }
        }

        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        float delta = (float) (now - lastTime) / 1000000000.f;
        float fps = 1.f / delta;

        if (delta > 0.2) delta = 0.2;
        world.step(delta);
        lastTime = now;

        statsText.setString(std::string("FPS: ") + std::to_string(fps) +
            "\n" + world.getStats());

        window.clear();
        window.draw(world);
        window.draw(statsText);
        window.display();
    }

    return 0;
}