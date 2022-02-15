#include "world/world.h"
#include <cmath>
#include <thread>
#include <utility>
#include <iostream>
#include "utils.h"

#define PI_f 3.14159265359f

void World::updateTerritories(float delta)
{
    // Reuse this
    static std::vector<uint32_t> cellCounts(settings.numTeams);
    static std::vector<float> ownershipTarget(settings.numTeams);

    for (int x = 0; x < settings.numChunks.x; x++)
    {
        for (int y = 0; y < settings.numChunks.y; y++)
        {
            float claimSpeed = 1.f;
            auto &chunk = getChunk({x, y});

            int numClaimable = 0;
            for (int i = 0; i < settings.numTeams; i++)
            {
                if (chunk->claimable[i])
                    numClaimable++;
            }

            uint32_t total = 0;
            for (int i = 0; i < settings.numTeams; i++)
            {
                auto count = chunk->cells[i].size();

                // Automatically claim undisputed territory, except for spawns
                if (numClaimable == 1 && chunk->claimable[i] && !chunk->isASpawn)
                {
                    count++;
                    claimSpeed = 5.f;
                }

                if (count > 0 && chunk->claimable[i])
                {
                    total += count;
                    cellCounts[i] = count;
                } else cellCounts[i] = 0;
            }

            if (total != 0)
            {
                auto finalOwnership = chunk->teamOwnership;
                for (int i = 0; i < settings.numTeams; i++)
                {
                    ownershipTarget[i] = (float) cellCounts[i] / (float) total;

                    // Move towards ownershipTarget
                    if (finalOwnership[i] > ownershipTarget[i])
                    {
                        finalOwnership[i] -= delta * claimSpeed;
                        finalOwnership[i] = clamp(finalOwnership[i], ownershipTarget[i], 1.f);
                    } else
                    {
                        finalOwnership[i] += delta * claimSpeed;
                        finalOwnership[i] = clamp(finalOwnership[i], 0.f, ownershipTarget[i]);
                    }
                }

                updateChunkOwnership({x, y}, chunk, finalOwnership);
                updateTerritoryColor({x, y}, chunk);
            }
        }
    }

    for (sf::Vector3i v: claimabilityUpdateQueue)
    {
        int teamId = v.x;
        int x = v.y;
        int y = v.z;

        if (!dirtyTeams[teamId]) partialUpdateChunkClaimability({x, y}, teamId);
    }
    claimabilityUpdateQueue.clear();

    for (sf::Vector3i v: setClaimableQueue)
    {
        int teamId = v.x;
        int x = v.y;
        int y = v.z;

        if (!dirtyTeams[teamId]) getChunk({x, y})->claimable[teamId] = true;
    }
    setClaimableQueue.clear();

    for (int i = 0; i < settings.numTeams; i++)
    {
        if (dirtyTeams[i])
        {
            clearChunkClaimability(i);
            floodUpdateTerritory(worldToChunkPos(settings.teamSpawns[i]), i);
            dirtyTeams[i] = false;
        }
    }
}

void World::updateChunkOwnership(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk,
                                 const std::vector<float> &newOwnership)
{
    int initialOwner = chunk->getCurrentOwner();
    int finalOwner = -1;
    for (int i = 0; i < settings.numTeams; i++) // TODO: Optimize first if branch to also break
        if (newOwnership[i] == 1.f) finalOwner = i;
        else if (newOwnership[i] != 0.f) break;

    if (initialOwner != finalOwner)
    {
        if (initialOwner == -1)
        {
            // Initially unowned, now owned. Connection may have been created.
            if (checkConnectionCreated(chunkPos, chunk, finalOwner))
            {
                chunk->teamOwnership = newOwnership;
                // Update entire team
                dirtyTeams[finalOwner] = true;
            } else
            {
                chunk->teamOwnership = newOwnership;
                // Update only adjacent chunks
                if (chunkPos.y + 1 < settings.numChunks.y)
                    setClaimableQueue.emplace_back(finalOwner, chunkPos.x, chunkPos.y + 1);
                if (chunkPos.y - 1 >= 0)
                    setClaimableQueue.emplace_back(finalOwner, chunkPos.x, chunkPos.y - 1);
                if (chunkPos.x + 1 < settings.numChunks.x)
                    setClaimableQueue.emplace_back(finalOwner, chunkPos.x + 1, chunkPos.y);
                if (chunkPos.x - 1 >= 0)
                    setClaimableQueue.emplace_back(finalOwner, chunkPos.x - 1, chunkPos.y);
            }
        } else if (finalOwner == -1)
        {
            // Now unowned, connection may have been severed
            chunk->teamOwnership = newOwnership;

            if (chunk->isASpawn)
            {
                for (int i = 0; i < settings.numTeams; i++)
                {
                    if (chunkPos == worldToChunkPos(settings.teamSpawns[i]))
                    {
                        dirtyTeams[i] = true;
                        return;
                    }
                }
            }

            if (checkConnectionSevered(chunkPos, chunk, initialOwner))
                // Update entire team
                dirtyTeams[initialOwner] = true;
            else
            {
                // Update only adjacent chunks
                if (chunkPos.y + 1 < settings.numChunks.y)
                    claimabilityUpdateQueue.emplace_back(initialOwner, chunkPos.x, chunkPos.y + 1);
                if (chunkPos.y - 1 >= 0)
                    claimabilityUpdateQueue.emplace_back(initialOwner, chunkPos.x, chunkPos.y - 1);
                if (chunkPos.x + 1 < settings.numChunks.x)
                    claimabilityUpdateQueue.emplace_back(initialOwner, chunkPos.x + 1, chunkPos.y);
                if (chunkPos.x - 1 >= 0)
                    claimabilityUpdateQueue.emplace_back(initialOwner, chunkPos.x - 1, chunkPos.y);
            }
        } else
        {
            // Was and still is fully owned, but owners changed.
            // Change should occur gradually but this case is included in case logic is changed.
            // TODO
            std::exit(-1);
        }
    } else chunk->teamOwnership = newOwnership;
}

bool World::checkConnectionSevered(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk, int teamId)
{
    int x = chunkPos.x, y = chunkPos.y;

    auto targets = std::vector<sf::Vector2i>();
    targets.reserve(settings.numTeams);

    if (x + 1 < settings.numChunks.x && getChunk(sf::Vector2i(x + 1, y))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x + 1, y);
    if (x - 1 >= 0 && getChunk(sf::Vector2i(x - 1, y))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x - 1, y);
    if (y + 1 < settings.numChunks.y && getChunk(sf::Vector2i(x, y + 1))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x, y + 1);
    if (y - 1 >= 0 && getChunk(sf::Vector2i(x, y - 1))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x, y - 1);

    if (targets.size() > 1)
    {
        auto p1 = targets.back();
        targets.pop_back();

        auto nowConnection = this->isEdgeConnectionApprox(p1, targets, {x, y}, teamId);

        if (std::any_of(nowConnection.begin(), nowConnection.end(), [](bool b)
        { return !b; }))
        {
            // A connection has been severed.
            return true;
        }
    }
    return false;
}

bool World::checkConnectionCreated(sf::Vector2i chunkPos, const std::unique_ptr<Chunk> &chunk, int teamId)
{
    int x = chunkPos.x, y = chunkPos.y;

    auto targets = std::vector<sf::Vector2i>();
    targets.reserve(settings.numTeams);

    if (x + 1 < settings.numChunks.x && getChunk(sf::Vector2i(x + 1, y))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x + 1, y);
    if (x - 1 >= 0 && getChunk(sf::Vector2i(x - 1, y))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x - 1, y);
    if (y + 1 < settings.numChunks.y && getChunk(sf::Vector2i(x, y + 1))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x, y + 1);
    if (y - 1 >= 0 && getChunk(sf::Vector2i(x, y - 1))->teamOwnership[teamId] == 1.f)
        targets.emplace_back(x, y - 1);

    if (targets.size() > 1)
    {
        auto p1 = targets.back();
        targets.pop_back();

        auto wasConnection = this->isEdgeConnectionApprox(p1, targets, {x, y}, teamId);

        if (std::any_of(wasConnection.begin(), wasConnection.end(), [](bool b)
        { return !b; }))
        {
            // A new connection has been created.
            return true;
        }
    }
    return false;
}

void World::updateTerritoryColor(sf::Vector2i pos, const std::unique_ptr<Chunk> &chunk)
{
    sf::Vector3f colorVec;
    for (int i = 0; i < settings.numTeams; i++)
    {
        colorVec.x += (float) settings.teamColors[i].r * chunk->teamOwnership[i];
        colorVec.y += (float) settings.teamColors[i].g * chunk->teamOwnership[i];
        colorVec.z += (float) settings.teamColors[i].b * chunk->teamOwnership[i];
    }
    sf::Color color = sf::Color::Black;
    color.r = (uint8_t) colorVec.x;
    color.g = (uint8_t) colorVec.y;
    color.b = (uint8_t) colorVec.z;
    color.a = 127;

    territoryMap->setPixel(pos.x, pos.y, color);
}

std::vector<bool>
World::isEdgeConnectionApprox(sf::Vector2i p1, std::vector<sf::Vector2i> targets, sf::Vector2i center, int teamId)
{
    // Walks clockwise
    sf::Vector2i w1 = p1;
    sf::Vector2i w1Dir = {(p1 - center).y, -(p1 - center).x};
    sf::Vector2i w1Pref = center - p1;

    // Walks counterclockwise
    sf::Vector2i w2 = p1;
    sf::Vector2i w2Dir = -w1Dir;
    sf::Vector2i w2Pref = center - p1;

    int nOut = 0;
    auto out = std::vector<bool>(targets.size());
    for (int i = 0; i < 8; i++)
    {
        // Update w1
        // Try to go in w1Pref
        if (inBoundsEx(w1 + w1Pref, {0, 0}, settings.numChunks) &&
            getChunk(w1 + w1Pref)->teamOwnership[teamId] == 1.f)
        {
            w1 += w1Pref;
            w1Dir = {w1Dir.y, -w1Dir.x};
            w1Pref = {w1Pref.y, -w1Pref.x};
        }
            // Then try to go in w1Dir
        else if (inBoundsEx(w1 + w1Dir, {0, 0}, settings.numChunks) &&
                 getChunk(w1 + w1Dir)->teamOwnership[teamId] == 1.f)
        {
            w1 += w1Dir;
        }
            // Then turn left when all else fails
        else
        {
            w1Dir = {-w1Dir.y, w1Dir.x};
            w1Pref = {-w1Pref.y, w1Pref.x};
        }
        for (int i = 0; i < targets.size(); i++)
            if (targets[i] == w1 && !out[i])
            {
                out[i] = true;
                nOut++;
                break;
            }

        if (nOut == targets.size()) return out;

        // Update w2
        // Try to go in w2Pref
        if (inBoundsEx(w2 + w2Pref, {0, 0}, settings.numChunks) &&
            getChunk(w2 + w2Pref)->teamOwnership[teamId] == 1.f)
        {
            w2 += w2Pref;
            w2Dir = {-w2Dir.y, w2Dir.x};
            w2Pref = {-w2Pref.y, w2Pref.x};
        }
            // Then try to go in w2Dir
        else if (inBoundsEx(w2 + w2Dir, {0, 0}, settings.numChunks) &&
                 getChunk(w2 + w2Dir)->teamOwnership[teamId] == 1.f)
        {
            w2 += w2Dir;
        }
            // Then turn right when all else fails
        else
        {
            w2Dir = {w2Dir.y, -w2Dir.x};
            w2Pref = {w2Pref.y, -w2Pref.x};
        }
        for (int i = 0; i < targets.size(); i++)
            if (targets[i] == w2 && !out[i])
            {
                out[i] = true;
                nOut++;
                break;
            }

        if (nOut == targets.size()) return out;
    }
    return out;
}

void World::clearChunkClaimability(int teamId)
{
    for (auto &chunk: chunks)
        chunk->claimable[teamId] = false;
}

void World::floodUpdateTerritory(sf::Vector2i root, int teamId)
{
    std::unique_ptr<std::list<sf::Vector2i>> stack = std::make_unique<std::list<sf::Vector2i>>();
    stack->push_back(root);
    while (!stack->empty())
    {
        sf::Vector2i p = stack->front();
        stack->pop_front();

        if (!inBoundsEx(p, {0, 0}, settings.numChunks))
            continue;

        auto &chunk = getChunk(p);
        if (chunk->claimable[teamId]) continue;
        chunk->claimable[teamId] = true;
        if (chunk->teamOwnership[teamId] == 1.f)
        {
            stack->push_back(sf::Vector2i(p.x + 1, p.y));
            stack->push_back(sf::Vector2i(p.x - 1, p.y));
            stack->push_back(sf::Vector2i(p.x, p.y + 1));
            stack->push_back(sf::Vector2i(p.x, p.y - 1));
        }
    }
}

void World::partialUpdateChunkClaimability(sf::Vector2i chunkPos, int teamId)
{
    auto &chunk = getChunk(chunkPos);

    if (chunkPos.y + 1 < settings.numChunks.y)
    {
        auto &offChunk = getChunk({chunkPos.x, chunkPos.y + 1});
        if (offChunk->teamOwnership[teamId] == 1.f && offChunk->claimable[teamId])
        {
            chunk->claimable[teamId] = true;
            return;
        }
    }
    if (chunkPos.y - 1 >= 0)
    {
        auto &offChunk = getChunk({chunkPos.x, chunkPos.y - 1});
        if (offChunk->teamOwnership[teamId] == 1.f && offChunk->claimable[teamId])
        {
            chunk->claimable[teamId] = true;
            return;
        }
    }
    if (chunkPos.x + 1 < settings.numChunks.x)
    {
        auto &offChunk = getChunk({chunkPos.x + 1, chunkPos.y});
        if (offChunk->teamOwnership[teamId] == 1.f && offChunk->claimable[teamId])
        {
            chunk->claimable[teamId] = true;
            return;
        }
    }
    if (chunkPos.y - 1 >= 0)
    {
        auto &offChunk = getChunk({chunkPos.x - 1, chunkPos.y});
        if (offChunk->teamOwnership[teamId] == 1.f && offChunk->claimable[teamId])
        {
            chunk->claimable[teamId] = true;
            return;
        }
    }
    chunk->claimable[teamId] = false;
}

void World::updateVelocities(float delta)
{
    float cellViewRange = 2;

    for (auto &c: cells)
    {
        auto centerPos = worldToChunkPos(c->position);
        int rectRadius = (int) ceilf(cellViewRange);

        sf::Vector2f targetVelocity = {0, 0};

        for (int ox = -rectRadius; ox <= rectRadius; ox++)
        {
            for (int oy = -rectRadius; oy <= rectRadius; oy++)
            {
                sf::Vector2i offsetPos = {ox + centerPos.x, oy + centerPos.y};
                if (!inBoundsEx(offsetPos, {0, 0}, settings.numChunks))
                    continue;

                int distSq = ox * ox + oy * oy;
                auto &chunk = getChunk(offsetPos);
                if ((float) distSq <= cellViewRange * cellViewRange && chunk->claimable[c->teamId])
                {
                    float weight =
                            (1 - chunk->teamOwnership[c->teamId]) / ((float) chunk->cells[c->teamId].size() + 1.f);
                    auto squareDist = (float) (ox * ox + oy * oy);
                    sf::Vector2f vecWeight = sf::Vector2f((float) ox, (float) oy) / (squareDist + 1);
                    targetVelocity += weight * vecWeight;
                }
            }
        }

        targetVelocity += c->preferredVelocity / 1000.f;
        targetVelocity /= (sqrtf(targetVelocity.x * targetVelocity.x + targetVelocity.y * targetVelocity.y) + 1e-8f);
        targetVelocity *= 50.f;
        c->velocity = (1 - delta) * c->velocity + delta * targetVelocity;
    }
}

void World::updatePositions(float delta)
{
    for (auto &cell: cells)
    {
        sf::Vector2f newPos = cell->position + cell->velocity * delta;
        if (newPos.x < 0)
        {
            newPos.x = 0;
            cell->velocity.x *= -1;
            cell->preferredVelocity.x *= -1;
        } else if (newPos.x >= (float) settings.width - 1e-4f)
        {
            newPos.x = (float) settings.width - 1e-4f;
            cell->velocity.x *= -1;
            cell->preferredVelocity.x *= -1;
        }
        if (newPos.y < 0)
        {
            newPos.y = 0;
            cell->velocity.y *= -1;
            cell->preferredVelocity.y *= -1;
        } else if (newPos.y >= (float) settings.height - 1e-4f)
        {
            newPos.y = (float) settings.height - 1e-4f;
            cell->velocity.y *= -1;
            cell->preferredVelocity.y *= -1;
        }
        this->updateCellPosition(cell, newPos);
    }
}

void World::attackNearby(float delta)
{
    // Attack
    for (auto &c: cells)
    {
        auto closestEnemy = findNearestEnemies(*c, settings.cellAttackRange);
        if (closestEnemy == nullptr) continue;

        float damageMul = c->strength / closestEnemy->strength * 0.1f;

        closestEnemy->health -= delta * damageMul;
        c->food += delta * damageMul;

        if (closestEnemy->health < 0)
        {
            c->food += closestEnemy->health;
            closestEnemy->health = 0;
        }
    }

    // Kill dead cells, always incrementing iterator before deletion.
    for (auto it = cells.begin(); it != cells.end();)
    {
        if (it->get()->health <= 0)
            deleteCell(it++);
        else it++;
    }

    // Spawn children
    static std::uniform_real_distribution<float> angleDistrib(0.f, PI_f * 2);
    static std::uniform_real_distribution<float> distrib01(0.f, 1);
    static std::uniform_real_distribution<float> strengthMulDist(0.8f, 1.25f);
    static std::uniform_real_distribution<float> velocityDistrib(-1.f, 1);
    static std::uniform_int_distribution<int> seedDistrib(-(1 << 30), 1 << 30);
    for (auto &parent: cells)
    {
        if (parent->food >= 1)
        {
            parent->food -= 1;

            float angle = angleDistrib(this->generator);
            float dist = sqrtf(distrib01(this->generator)) * 3.f;

            sf::Vector2f position = {
                    cosf(angle) * dist + parent->position.x,
                    sinf(angle) * dist + parent->position.y
            };
            position = clamp(position, {0, 0},
                             {(float) settings.width - 1e-4f, (float) settings.height - 1e-4f});

            sf::Vector2f velocity = {velocityDistrib(generator), velocityDistrib(generator)};
            sf::Vector2f preferredVelocity = {cosf(angle), sinf(angle)};

            auto strengthMul = strengthMulDist(generator);

            auto child = std::make_shared<Cell>(parent->teamId, seedDistrib(generator),
                                                parent->strength * strengthMul, 1, 0,
                                                velocity, preferredVelocity, position);
            sf::Vector2i chunkPos = worldToChunkPos(child->position);
            this->cells.push_back(child);
            auto &chunk = this->getChunk(chunkPos);
            chunk->cells[child->teamId].push_back(child);
        }
    }
}

void World::deleteCell(const std::list<std::shared_ptr<Cell>>::iterator &it)
{
    auto elem = *it;

    auto &chunkCells = getChunk(worldToChunkPos(elem->position))->cells[elem->teamId];
    chunkCells.erase(std::find(chunkCells.begin(), chunkCells.end(), elem));

    cells.erase(it);

}

sf::Vector2i World::worldToChunkPos(sf::Vector2f position) const
{
    int cx = (int) (position.x / settings.pixelsPerChunk);
    int cy = (int) (position.y / settings.pixelsPerChunk);
    return {cx, cy};
}

void World::updateCellPosition(const std::shared_ptr<Cell> &cell, sf::Vector2f newPosition)
{
    auto oldChunkPos = worldToChunkPos(cell->position);
    auto newChunkPos = worldToChunkPos(newPosition);
    cell->position = newPosition;
    if (newChunkPos != oldChunkPos)
    {
        auto &oldCells = getChunk(oldChunkPos)->cells[cell->teamId];
        auto &newCells = getChunk(newChunkPos)->cells[cell->teamId];
        newCells.push_back(cell);
        oldCells.erase(std::find(oldCells.begin(), oldCells.end(), cell));
    }
}

std::shared_ptr<Cell> World::findNearestEnemies(const Cell &cell, float maxDistance)
{
    int searchDistance = (int) ceilf(maxDistance / settings.pixelsPerChunk);

    sf::Vector2i chunkPos = worldToChunkPos(cell.position);

    std::shared_ptr<Cell> bestMatch = nullptr;
    float bestMatchDist = maxDistance;

    for (int ox = -searchDistance; ox <= searchDistance; ox++)
    {
        for (int oy = -searchDistance; oy <= searchDistance; oy++)
        {
            sf::Vector2i offsetPos = {ox + chunkPos.x, oy + chunkPos.y};
            if (!inBoundsEx(offsetPos, {0, 0}, settings.numChunks))
                continue;

            auto &chunk = getChunk(offsetPos);
            for (int i = 0; i < settings.numTeams; i++)
            {
                if (i == cell.teamId) continue;

                for (auto &otherCell: chunk->cells[i])
                {
                    auto cellOffset = otherCell->position - cell.position;
                    auto cellDistance = sqrtf(cellOffset.x * cellOffset.x + cellOffset.y * cellOffset.y);

                    if (cellDistance < bestMatchDist)
                    {
                        bestMatch = otherCell;
                        bestMatchDist = cellDistance;
                    }
                }
            }
        }
    }
    return bestMatch;
}

std::shared_ptr<Cell> World::findNearestFriendly(const Cell &cell, float maxDistance)
{
    int searchDistance = (int) ceilf(maxDistance / settings.pixelsPerChunk);

    sf::Vector2i chunkPos = worldToChunkPos(cell.position);

    std::shared_ptr<Cell> bestMatch = nullptr;
    float bestMatchDist = maxDistance;

    for (int ox = -searchDistance; ox <= searchDistance; ox++)
    {
        for (int oy = -searchDistance; oy <= searchDistance; oy++)
        {
            sf::Vector2i offsetPos = {ox + chunkPos.x, oy + chunkPos.y};
            if (!inBoundsEx(offsetPos, {0, 0}, settings.numChunks))
                continue;

            auto &chunk = getChunk(offsetPos);
            for (int i = 0; i < settings.numTeams; i++)
            {
                if (i != cell.teamId) continue;

                for (auto &otherCell: chunk->cells[i])
                {
                    auto cellOffset = otherCell->position - cell.position;
                    auto cellDistance = sqrtf(cellOffset.x * cellOffset.x + cellOffset.y * cellOffset.y);

                    if (cellDistance < bestMatchDist)
                    {
                        bestMatch = otherCell;
                        bestMatchDist = cellDistance;
                    }
                }
            }
        }
    }
    return bestMatch;
}

const std::unique_ptr<Chunk> &World::getChunk(sf::Vector2i position) const
{
    return this->chunks[position.x + position.y * settings.numChunks.x];
}

//
// PUBLIC FUNCTIONS
//

World::World(WorldSettings settings, int seed) :
        settings(std::move(settings)), pool((int) std::thread::hardware_concurrency()),
        generator(seed), dirtyTeams(settings.numTeams)
{
    this->settings.numChunks.x = (int) ceilf((float) this->settings.width / (float) this->settings.pixelsPerChunk);
    this->settings.numChunks.y = (int) ceilf((float) this->settings.height / (float) this->settings.pixelsPerChunk);

    chunks = std::vector<std::unique_ptr<Chunk>>(this->settings.numChunks.x * this->settings.numChunks.y);
    for (int i = 0; i < chunks.size(); i++)
        // Set isASpawn to false initially, will be updated
        chunks[i] = std::make_unique<Chunk>(this->settings.numTeams, false);

    for (int i = 0; i < settings.numTeams; i++)
    {
        getChunk(worldToChunkPos(this->settings.teamSpawns[i]))->isASpawn = true;
    }

    this->territoryMap->create(this->settings.numChunks.x, this->settings.numChunks.y);

    std::uniform_real_distribution<float> angleDistrib(0.f, PI_f * 2);
    std::uniform_real_distribution<float> distrib01(0.f, 1);
    std::uniform_real_distribution<float> velocityDistrib(-1.f, 1);
    std::uniform_int_distribution<int> seedDistrib(-(1 << 30), 1 << 30);
    for (int teamId = 0; teamId < this->settings.numTeams; teamId++)
    {
        for (int i = 0; i < this->settings.initialCellsPerTeam; i++)
        {
            float angle = angleDistrib(this->generator);
            float dist = sqrtf(distrib01(this->generator)) * this->settings.spawnRadius;

            sf::Vector2f position = {
                    cosf(angle) * dist + this->settings.teamSpawns[teamId].x,
                    sinf(angle) * dist + this->settings.teamSpawns[teamId].y
            };

            sf::Vector2f velocity = {velocityDistrib(generator), velocityDistrib(generator)};
            sf::Vector2f prefferedVelocity = {cosf(angle), sinf(angle)};
            auto c = std::make_shared<Cell>(teamId, seedDistrib(generator), 1, 1, 0,
                                            velocity, prefferedVelocity, position);
            sf::Vector2i chunkPos = worldToChunkPos(c->position);
            this->cells.push_back(c);
            auto &chunk = this->getChunk(chunkPos);
            chunk->cells[c->teamId].push_back(c);

            if (chunk->teamOwnership[c->teamId] != 1.f)
            {
                for (int k = 0; k < this->settings.numTeams; k++)
                    chunk->teamOwnership[k] = k == c->teamId ? 1.f : 0.f;
            }
        }
    }

    for (int i = 0; i < this->settings.numTeams; i++)
        floodUpdateTerritory(worldToChunkPos(this->settings.teamSpawns[i]), i);
}

void World::step(float delta)
{
    this->worldTime += delta;

    updateTerritories(delta);
    updateVelocities(delta);
    updatePositions(delta);
    attackNearby(delta);
}

void World::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    sf::CircleShape circle(settings.cellRadius, 8);
    circle.setOrigin(settings.cellRadius, settings.cellRadius);

    if (viewMode == ViewMode::DEFAULT)
    {
        sf::Texture t = sf::Texture();
        t.loadFromImage(*this->territoryMap);
        sf::Sprite sprite(t);
        sprite.setScale(settings.pixelsPerChunk, settings.pixelsPerChunk);

        target.draw(sprite, states);
    } else if (viewMode == ViewMode::CLAIMABLE)
    {
        sf::Image claimableImg = sf::Image();
        claimableImg.create(settings.numChunks.x, settings.numChunks.y);
        for (int x = 0; x < settings.numChunks.x; x++)
        {
            for (int y = 0; y < settings.numChunks.y; y++)
            {
                auto &chunk = getChunk(sf::Vector2i(x, y));
                float totalClaimable = 0;
                for (int i = 0; i < settings.numTeams; i++)
                    if (chunk->claimable[i]) totalClaimable++;
                if (totalClaimable == 0) continue;
                sf::Vector3f colorVec;
                for (int i = 0; i < settings.numTeams; i++)
                {
                    colorVec.x += (float) settings.teamColors[i].r * (chunk->claimable[i] ? 1.f : 0.f) / totalClaimable;
                    colorVec.y += (float) settings.teamColors[i].g * (chunk->claimable[i] ? 1.f : 0.f) / totalClaimable;
                    colorVec.z += (float) settings.teamColors[i].b * (chunk->claimable[i] ? 1.f : 0.f) / totalClaimable;
                }
                claimableImg.setPixel(x, y,
                                      sf::Color((uint8_t) colorVec.x, (uint8_t) colorVec.y, (uint8_t) colorVec.z, 127));
            }
        }

        sf::Texture t = sf::Texture();
        t.loadFromImage(claimableImg);
        sf::Sprite sprite(t);
        sprite.setScale(settings.pixelsPerChunk, settings.pixelsPerChunk);

        target.draw(sprite, states);
    }
    // else impossible

    for (const auto &c: cells)
    {
        auto pos = c->position;
        circle.setPosition(pos);
        auto color = settings.teamColors[c->teamId];
        color.a = (uint8_t) lerp(150.f, 255.f, c->health);
        circle.setFillColor(color);
        target.draw(circle, states);
    }
}

World::~World()
{

}
