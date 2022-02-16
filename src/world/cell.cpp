#include "world/cell.h"

Cell::Cell(int teamId, int seed, float strength, float health, float supply, float targetSupply,
           sf::Vector2f velocity, sf::Vector2f preferredVelocity, sf::Vector2f position)
{
    this->teamId = teamId;
    this->seed = seed;
    this->strength = strength;
    this->health = health;
    this->supply = supply;
    this->targetSupply = targetSupply;
    this->velocity = velocity;
    this->preferredVelocity = preferredVelocity;
    this->position = position;
}
