#ifndef CELL_BATTLES_CELL_H
#define CELL_BATTLES_CELL_H

#include <list>
#include <memory>
#include <SFML/System.hpp>

struct Cell
{
    int teamId;
    int seed;
    float strength;
    float health;
    float supply;
    float lastBirth = -1;
    sf::Vector2f velocity;
    sf::Vector2f preferredVelocity;
    sf::Vector2f position;

    Cell(int teamId, int seed, float strength, float health, float supply, sf::Vector2f velocity,
         sf::Vector2f preferredVelocity, sf::Vector2f position);
};


#endif //CELL_BATTLES_CELL_H
