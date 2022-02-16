#ifndef CELL_BATTLES_WORLD_H
#define CELL_BATTLES_WORLD_H

#include <SFML/Graphics.hpp>
#include "cell.h"
#include <list>
#include <random>
#include "ctpl_stl.h"
#include "chunk.h"
#include "view_mode.h"
#include "world_settings.h"

class World : public sf::Drawable
{
    WorldSettings settings;

    std::vector<std::unique_ptr<Chunk>> chunks;
    float maxSupplyGeneration = -1.f;
    std::list<std::shared_ptr<Cell>> cells;
    float worldTime = 0;

    std::default_random_engine generator;

    ctpl::thread_pool pool;

    // Searches could probably be improved with an octree
    std::unique_ptr<sf::Image> territoryMap = std::make_unique<sf::Image>();

    std::vector<sf::Vector2i> walkOrder;


    void updateTerritories(float delta);

    void updateTerritoryColor(sf::Vector2i pos, const std::unique_ptr<Chunk>& chunk);

    void updateChunkSupply(float delta);

    void updateCellSupply(float delta);

    void updateVelocities(float delta);

    void updatePositions(float delta);

    void attackNearby(float delta);

    void deleteDeadCells();

    void spawnChildren(float delta);

    void deleteCell(const std::list<std::shared_ptr<Cell>>::iterator& it);

    // Updates cell position. Will also update chunks the cell is in, or moves to.
    void updateCellPosition(const std::shared_ptr<Cell>& cell, sf::Vector2f newPosition);

    std::shared_ptr<Cell> findNearestEnemies(const Cell& cell, float maxDistance);

    std::shared_ptr<Cell> findNearestFriendly(const Cell& cell, float maxDistance);

    const std::unique_ptr<Chunk>& getChunk(sf::Vector2i pos) const;

    bool isEdge(sf::Vector2i chunkPos, int teamId);

    bool isClaimable(sf::Vector2i chunkPos, int teamId);

public:
    ViewMode viewMode = ViewMode::DEFAULT;

    World(WorldSettings settings, int seed);

    ~World() override;

    void step(float delta);

    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    sf::Vector2i worldToChunkPos(sf::Vector2f position) const;
};

#endif //CELL_BATTLES_WORLD_H
