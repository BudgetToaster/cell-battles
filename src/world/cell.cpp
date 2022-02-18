#include "world/cell.h"

Cell::Cell(int teamId, int seed, float attack, float defense, float speed, float metabolism,
           float health, float supply, float targetSupply,
           sf::Vector2f velocity, sf::Vector2f preferredVelocity, sf::Vector2f position)
{
    this->teamId = teamId;
    this->seed = seed;

    this->attack = attack;
    this->defense = defense;
    this->speed = speed;
    this->metabolism = metabolism;

    this->health = health;
    this->supply = supply;
    this->velocity = velocity;
    this->preferredVelocity = preferredVelocity;
    this->position = position;
}
