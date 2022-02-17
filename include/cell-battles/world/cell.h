#ifndef CELL_BATTLES_CELL_H
#define CELL_BATTLES_CELL_H

#include <list>
#include <memory>
#include <SFML/System.hpp>

struct Cell
{
    int teamId;
    int seed;

    float attack;
    float defense;
    float speed;
    float metabolism;

    float health;
    float supply;
    float targetSupply;
    float lastBirth = 0;
    sf::Vector2f velocity;
    sf::Vector2f preferredVelocity;
    sf::Vector2f position;

    Cell(int teamId, int seed, float attack, float defense, float speed, float metabolism,
         float health, float supply, float targetSupply, sf::Vector2f velocity,
         sf::Vector2f preferredVelocity, sf::Vector2f position);
};


#endif //CELL_BATTLES_CELL_H
