#ifndef CELL_BATTLES_WORLD_SETTINGS_H
#define CELL_BATTLES_WORLD_SETTINGS_H

#include <vector>
#include <SFML/Graphics.hpp>

struct WorldSettings
{
    friend class World;

private:
    sf::Vector2i numChunks;

public:
    // Screen aspect ratio (width / height)
    int width;
    int height;

    float pixelsPerChunk;

    // Radius of cells when rendered.
    float cellRadius;

    // Maximum distance for cells to attack.
    float cellAttackRange;

    // Number of cell teams
    int numTeams;

    // Number of cells to spawn per team at start
    int initialCellsPerTeam;

    // Color of each team
    std::vector<sf::Color> teamColors;

    // Center of the spawn circle
    std::vector<sf::Vector2f> teamSpawns;

    // Radius of the spawn circle
    float spawnRadius;
};

#endif //CELL_BATTLES_WORLD_SETTINGS_H
