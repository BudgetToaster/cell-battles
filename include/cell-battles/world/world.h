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

    std::vector<bool> dirtyTeams;
    std::vector<std::unique_ptr<Chunk>> chunks;
    std::vector<sf::Vector3i> claimabilityUpdateQueue;
    std::vector<sf::Vector3i> setClaimableQueue;
    std::list<std::shared_ptr<Cell>> cells;
    float worldTime = 0;

    std::default_random_engine generator;

    ctpl::thread_pool pool;

    // Searches could probably be improved with an octree
    std::unique_ptr<sf::Image> territoryMap = std::make_unique<sf::Image>();


    void updateTerritories(float delta);

    std::vector<bool>
    isEdgeConnectionApprox(sf::Vector2i p1, std::vector<sf::Vector2i> targets, sf::Vector2i center, int teamId);

    void updateChunkOwnership(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk,
                              const std::vector<float> &newOwnership);

    bool checkConnectionSevered(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk, int teamId);

    bool checkConnectionCreated(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk, int teamId);

    void updateTerritoryColor(sf::Vector2i pos, const std::unique_ptr<Chunk> &chunk);

    void clearChunkClaimability(int teamId);

    void partialUpdateChunkClaimability(sf::Vector2i chunkPos, int teamId);

    void floodUpdateTerritory(sf::Vector2i root, int teamId);

    void updateVelocities(float delta);

    void updatePositions(float delta);

    void attackNearby(float delta);

    void deleteCell(const std::list<std::shared_ptr<Cell>>::iterator &it);

    // Updates cell position. Will also update chunks the cell is in, or moves to.
    void updateCellPosition(const std::shared_ptr<Cell> &cell, sf::Vector2f newPosition);

    std::shared_ptr<Cell> findNearestEnemies(const Cell &cell, float maxDistance);

    std::shared_ptr<Cell> findNearestFriendly(const Cell &cell, float maxDistance);

    const std::unique_ptr<Chunk> &getChunk(sf::Vector2i pos) const;

public:
    ViewMode viewMode = ViewMode::DEFAULT;

    World(WorldSettings settings, int seed);
    ~World() override;

    void step(float delta);

    void draw(sf::RenderTarget &target, sf::RenderStates states) const override;

    sf::Vector2i worldToChunkPos(sf::Vector2f position) const;
};

#endif //CELL_BATTLES_WORLD_H
