#include "world/world.h"
#include <cmath>
#include <thread>
#include <utility>
#include <iostream>
#include "utils.h"

#define PI_f 3.14159265359f

void World::updateTerritories(float delta)
{
    // Reuse these
    std::vector<uint32_t> cellCounts(settings.numTeams);
    std::vector<float> ownershipTarget(settings.numTeams);

    for (int x = 0; x < settings.numChunks.x; x++)
    {
        for (int y = 0; y < settings.numChunks.y; y++)
        {
            float claimSpeed = 1.f;
            auto& chunk = getChunk({x, y});

            uint32_t total = 0;
            for (int i = 0; i < settings.numTeams; i++)
            {
                auto count = chunk->cells[i].size();

                if (count > 0 && (isClaimable({x, y}, i)))
                {
                    total += count;
                    cellCounts[i] = count;
                }
                else cellCounts[i] = 0;
            }

            if (total != 0)
            {
                for (int i = 0; i < settings.numTeams; i++)
                {
                    ownershipTarget[i] = (float) cellCounts[i] / (float) total;

                    // Move towards ownershipTarget
                    if (chunk->teamOwnership[i] > ownershipTarget[i])
                    {
                        chunk->teamOwnership[i] -= delta * claimSpeed;
                        chunk->teamOwnership[i] = clamp(chunk->teamOwnership[i], ownershipTarget[i], 1.f);
                    }
                    else
                    {
                        chunk->teamOwnership[i] += delta * claimSpeed;
                        chunk->teamOwnership[i] = clamp(chunk->teamOwnership[i], 0.f, ownershipTarget[i]);
                    }
                }

                updateTerritoryColor({x, y}, chunk);
            }
        }
    }
}

void World::updateTerritoryColor(sf::Vector2i pos, const std::unique_ptr<Chunk>& chunk)
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

void World::developChunks(float delta)
{
    for(auto& chunk : chunks) {
        int owner = chunk->getCurrentOwner();

        if(chunk->getCurrentOwner() == -1)
        {
            chunk->development -= delta;
            if(chunk->development < 0) chunk->development = 0;
        }
        else
        {
            chunk->development += delta / 120.f;
            if(chunk->development > 1.f) chunk->development = 1.f;
        }
    }
}

void World::updateChunkSupply(float delta)
{
    static std::vector<int> ownerBuffer(settings.numChunks.x * settings.numChunks.y);
    static std::vector<float> transferBuffer(settings.numChunks.x * settings.numChunks.y);

    for (int x = 0; x < settings.numChunks.x; x++)
        for (int y = 0; y < settings.numChunks.y; y++)
            ownerBuffer[x + y * settings.numChunks.x] = getChunk({x, y})->getCurrentOwner();

    for (int x = 0; x < settings.numChunks.x; x++)
    {
        for (int y = 0; y < settings.numChunks.y; y++)
        {
            auto& curChunk = getChunk({x, y});
            auto curChunkOwner = ownerBuffer[x + y * settings.numChunks.x];

            if(curChunkOwner == -1)
            {
                transferBuffer[x + y * settings.numChunks.x] = std::max(10.f * -delta, -curChunk->supply);
                continue;
            }

            float westSupply = curChunk->supply;
            float eastSupply = curChunk->supply;
            float northSupply = curChunk->supply;
            float southSupply = curChunk->supply;

            if (x + 1 < settings.numChunks.x && ownerBuffer[(x + 1) + y * settings.numChunks.x] == curChunkOwner)
                eastSupply = getChunk({x + 1, y})->supply;
            if (x - 1 >= 0 && ownerBuffer[(x - 1) + y * settings.numChunks.x] == curChunkOwner)
                westSupply = getChunk({x - 1, y})->supply;
            if (y + 1 < settings.numChunks.y && ownerBuffer[x + (y + 1) * settings.numChunks.x] == curChunkOwner)
                southSupply = getChunk({x, y + 1})->supply;
            if (y - 1 >= 0 && ownerBuffer[x + (y - 1) * settings.numChunks.x] == curChunkOwner)
                northSupply = getChunk({x, y - 1})->supply;

            float dsdx2 = (eastSupply - curChunk->supply) - (curChunk->supply - westSupply);
            float dsdy2 = (southSupply - curChunk->supply) - (curChunk->supply - northSupply);

            float supplyTransfer = (dsdx2 + dsdy2) * settings.supplyDiffusionRate + curChunk->getEffectiveSupplyGeneration();

            transferBuffer[x + y * settings.numChunks.x] = supplyTransfer;
        }
    }


    for (int x = 0; x < settings.numChunks.x; x++)
    {
        for (int y = 0; y < settings.numChunks.y; y++)
        {
            auto& chunk = getChunk({x, y});
            chunk->supply += transferBuffer[x + y * settings.numChunks.x] * delta;
        }
    }
}

void World::updateCellSupply(float delta)
{
    for(auto& cell : cells) {
        float passiveLoss = delta * 0.03f * (cell->supply * cell->supply + 5.f);
        cell->supply -= passiveLoss;

        if(cell->supply < 0)
        {
            cell->health += cell->supply;
            cell->supply = 0;
        }

        auto& chunk = getChunk(worldToChunkPos(cell->position));
        if(chunk->getCurrentOwner() != cell->teamId) continue;
        auto t = std::min(delta, chunk->supply);
        cell->supply += t;
        chunk->supply -= t;
    }

    deleteDeadCells();
}

void World::updateVelocities(float delta)
{
    float cellViewRange = 2;

    for (auto& c: cells)
    {
        bool needSupply = c->supply < c->targetSupply;

        auto centerPos = worldToChunkPos(c->position);
        int rectRadius = (int) ceilf(cellViewRange);

        sf::Vector2f targetVelocity = {0, 0};

        for (int ox = -rectRadius; ox <= rectRadius; ox++)
        {
            for (int oy = -rectRadius; oy <= rectRadius; oy++)
            {
                if(ox == 0 && oy == 0) continue;

                sf::Vector2i offsetPos = {ox + centerPos.x, oy + centerPos.y};
                if (!inBoundsEx(offsetPos, {0, 0}, settings.numChunks))
                    continue;

                int distSq = ox * ox + oy * oy;
                auto& chunk = getChunk(offsetPos);

                bool isClaimed = chunk->teamOwnership[c->teamId] == 1.f;


                if ((float) distSq <= cellViewRange * cellViewRange)
                {
                    bool needsDefense = isEdge(offsetPos, c->teamId) ||
                            (isClaimable(offsetPos, c->teamId) && chunk->teamOwnership[c->teamId] < 1);

                    float weight;

                    if((needSupply && isClaimed) && needsDefense)
                    {
                        // Encourage cells to go to undefended areas
                        float uniformDefenseWeight = 1.f / ((float)chunk->cells[c->teamId].size() + 1.f);

                        weight = std::min(1.f, chunk->supply) * std::max(1.f, 10.f * uniformDefenseWeight);
                    }
                    else if(needSupply && isClaimed)
                    {
                        // Need supply but chunk doesnt need defense
                        weight = std::min(1.f, chunk->supply);
                    }
                    else if(needsDefense)
                    {
                        // Chunk needs defense and cell doesn't need supply

                        // Encourage cells to go to undefended areas
                        float uniformDefenseWeight = 1.f / ((float)chunk->cells[c->teamId].size() + 1.f);
                        weight = uniformDefenseWeight;
                    }
                    else
                    {
                        // Don't need supply and chunk doesn't need defense.
                        continue;
                    }

                    auto offsetDist = sqrtf((float)(ox * ox + oy * oy));
                    sf::Vector2f vecWeight = sf::Vector2f((float) ox, (float) oy) / (offsetDist);
                    targetVelocity += weight * vecWeight;
                }
            }
        }

        if(std::abs(targetVelocity.x) < 0.01f && std::abs(targetVelocity.y) < 0.01f)
            targetVelocity = c->preferredVelocity;
        auto targetVelocityMag = sqrtf(targetVelocity.x * targetVelocity.x + targetVelocity.y * targetVelocity.y);
        targetVelocity /= targetVelocityMag;
        targetVelocity *= 20.f;
        c->velocity = (1 - delta) * c->velocity + delta * targetVelocity;
    }
}

void World::updatePositions(float delta)
{
    for (auto& cell: cells)
    {
        sf::Vector2f newPos = cell->position + cell->velocity * delta;
        if (newPos.x < 0)
        {
            newPos.x = 0;
            cell->velocity.x *= -1;
            cell->preferredVelocity.x *= -1;
        }
        else if (newPos.x >= (float) settings.width - 1e-4f)
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
        }
        else if (newPos.y >= (float) settings.height - 1e-4f)
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
    for (auto& c: cells)
    {
        auto closestEnemy = findNearestEnemies(*c, settings.cellAttackRange);
        if (closestEnemy == nullptr) continue;

        float damageMul = c->strength / closestEnemy->strength * 0.3f;

        closestEnemy->health -= delta * damageMul;

        if (closestEnemy->health < 0)
        {
            closestEnemy->health = 0;
        }
    }

    deleteDeadCells();
}

void World::deleteDeadCells()
{
    // Kill dead cells, always incrementing iterator before deletion.
    for (auto it = cells.begin(); it != cells.end();)
    {
        if (it->get()->health <= 0)
            deleteCell(it++);
        else it++;
    }
}

void World::spawnChildren(float delta)
{
    static std::uniform_real_distribution<float> angleDistrib(0.f, PI_f * 2);
    static std::uniform_real_distribution<float> distrib01(0.f, 1);
    static std::uniform_real_distribution<float> strengthMulDist(0.8f, 1.25f);
    static std::uniform_real_distribution<float> velocityDistrib(-1.f, 1);
    static std::uniform_int_distribution<int> seedDistrib(-(1 << 30), 1 << 30);
    for (auto& parent: cells)
    {
        if (parent->supply >= 2.5 && parent->lastBirth < worldTime - settings.childSpawnDelay)
        {
            parent->supply -= 2;
            parent->lastBirth = worldTime;

            float angle = angleDistrib(this->generator);
            float dist = sqrtf(distrib01(this->generator)) * 3.f;

            sf::Vector2f position = {
                    cosf(angle) * dist + parent->position.x,
                    sinf(angle) * dist + parent->position.y
            };
            position = clamp(position, {0, 0},{(float) settings.width - 1e-4f, (float) settings.height - 1e-4f});

            sf::Vector2f velocity = {velocityDistrib(generator), velocityDistrib(generator)};
            sf::Vector2f preferredVelocity = {cosf(angle), sinf(angle)};

            auto strengthMul = strengthMulDist(generator);

            float targetSupply = distrib01(generator) > 0.5 ? 1.f : 3.f;

            auto child = std::make_shared<Cell>(parent->teamId, seedDistrib(generator),
                                                parent->strength * strengthMul, 1, 1, targetSupply,
                                                velocity, preferredVelocity, position);
            child->lastBirth = worldTime;
            sf::Vector2i chunkPos = worldToChunkPos(child->position);
            this->cells.push_back(child);
            auto& chunk = this->getChunk(chunkPos);
            chunk->cells[child->teamId].push_back(child);
        }
    }
}

void World::deleteCell(const std::list<std::shared_ptr<Cell>>::iterator& it)
{
    auto elem = *it;

    auto& chunkCells = getChunk(worldToChunkPos(elem->position))->cells[elem->teamId];
    chunkCells.erase(std::find(chunkCells.begin(), chunkCells.end(), elem));

    cells.erase(it);

}

sf::Vector2i World::worldToChunkPos(sf::Vector2f position) const
{
    int cx = (int) (position.x / settings.pixelsPerChunk);
    int cy = (int) (position.y / settings.pixelsPerChunk);
    return {cx, cy};
}

void World::updateCellPosition(const std::shared_ptr<Cell>& cell, sf::Vector2f newPosition)
{
    auto oldChunkPos = worldToChunkPos(cell->position);
    auto newChunkPos = worldToChunkPos(newPosition);
    cell->position = newPosition;
    if (newChunkPos != oldChunkPos)
    {
        auto& oldCells = getChunk(oldChunkPos)->cells[cell->teamId];
        auto& newCells = getChunk(newChunkPos)->cells[cell->teamId];
        newCells.push_back(cell);
        oldCells.erase(std::find(oldCells.begin(), oldCells.end(), cell));
    }
}

void World::floodClaim(sf::Vector2i center, int maxIters, int teamId)
{
    std::unique_ptr<std::list<sf::Vector2i>> stack = std::make_unique<std::list<sf::Vector2i>>();
    stack->push_back(center);

    int i = 0;
    while (!stack->empty() && i < maxIters)
    {
        sf::Vector2i p = stack->front();
        stack->pop_front();

        if (!inBoundsEx(p, {0, 0}, settings.numChunks))
            continue;

        auto &chunk = getChunk(p);
        if (chunk->getCurrentOwner() != -1) continue;
        chunk->teamOwnership[teamId] = 1.f;
        stack->push_back(sf::Vector2i(p.x + 1, p.y));
        stack->push_back(sf::Vector2i(p.x - 1, p.y));
        stack->push_back(sf::Vector2i(p.x, p.y + 1));
        stack->push_back(sf::Vector2i(p.x, p.y - 1));
        i++;
    }
}

std::shared_ptr<Cell> World::findNearestEnemies(const Cell& cell, float maxDistance)
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

            auto& chunk = getChunk(offsetPos);
            for (int i = 0; i < settings.numTeams; i++)
            {
                if (i == cell.teamId) continue;

                for (auto& otherCell: chunk->cells[i])
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

std::shared_ptr<Cell> World::findNearestFriendly(const Cell& cell, float maxDistance)
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

            auto& chunk = getChunk(offsetPos);
            for (int i = 0; i < settings.numTeams; i++)
            {
                if (i != cell.teamId) continue;

                for (auto& otherCell: chunk->cells[i])
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

const std::unique_ptr<Chunk>& World::getChunk(sf::Vector2i position) const
{
    return this->chunks[position.x + position.y * settings.numChunks.x];
}

bool World::isEdge(sf::Vector2i p, int teamId)
{
    bool friendlyConnected = false;
    bool enemyConnected = false;

    if(p.x + 1 < settings.numChunks.x)
    {
        if (getChunk({p.x + 1, p.y})->teamOwnership[teamId] == 1.f)
            friendlyConnected = true;
        else enemyConnected = true;
    }

    if(p.x - 1 >= 0)
    {
        if (getChunk({p.x - 1, p.y})->teamOwnership[teamId] == 1.f)
            friendlyConnected = true;
        else enemyConnected = true;
    }

    if(friendlyConnected && enemyConnected) return true;

    if(p.y + 1 < settings.numChunks.y)
    {
        if (getChunk({p.x, p.y + 1})->teamOwnership[teamId] == 1.f)
            friendlyConnected = true;
        else enemyConnected = true;
    }

    if(friendlyConnected && enemyConnected) return true;

    if(p.y - 1 >= 0)
    {
        if (getChunk({p.x, p.y - 1})->teamOwnership[teamId] == 1.f)
            friendlyConnected = true;
        else enemyConnected = true;
    }

    return friendlyConnected && enemyConnected;
}

bool World::isClaimable(sf::Vector2i p, int teamId)
{
    if(p.x + 1 < settings.numChunks.x && getChunk({p.x + 1, p.y})->teamOwnership[teamId] == 1.f)
        return true;
    if(p.x - 1 >= 0 && getChunk({p.x - 1, p.y})->teamOwnership[teamId] == 1.f)
        return true;
    if(p.y + 1 < settings.numChunks.y && getChunk({p.x, p.y + 1})->teamOwnership[teamId] == 1.f)
        return true;
    if(p.y - 1 >= 0 && getChunk({p.x, p.y - 1})->teamOwnership[teamId] == 1.f)
        return true;

    return false;
}

//
// PUBLIC FUNCTIONS
//

World::World(WorldSettings settings, int seed) :
        settings(std::move(settings)), pool((int) std::thread::hardware_concurrency()),
        generator(seed)
{
    this->settings.numChunks.x = (int) ceilf((float) this->settings.width / (float) this->settings.pixelsPerChunk);
    this->settings.numChunks.y = (int) ceilf((float) this->settings.height / (float) this->settings.pixelsPerChunk);

    walkOrder.reserve(this->settings.numChunks.x * this->settings.numChunks.y);
    for (int x = 0; x < this->settings.numChunks.x; x++)
    {
        for (int y = 0; y < this->settings.numChunks.y; y++)
        {
            walkOrder.emplace_back(x, y);
        }
    }
    for (int i = 0; i < walkOrder.size() - 1; i++)
    {
        std::uniform_int_distribution<int> indexDistrib(i, (int) walkOrder.size() - 1);
        auto randIdx = indexDistrib(generator);
        auto temp = walkOrder[randIdx];
        walkOrder[randIdx] = walkOrder[i];
        walkOrder[i] = temp;
    }

    chunks = std::vector<std::unique_ptr<Chunk>>(this->settings.numChunks.x * this->settings.numChunks.y);
    for (int i = 0; i < chunks.size(); i++)
        // Set isASpawn to false initially, will be updated
        chunks[i] = std::make_unique<Chunk>(this->settings.numTeams, false);

    this->territoryMap->create(this->settings.numChunks.x, this->settings.numChunks.y);


    for(int i = 0; i < this->settings.numTeams; i++)
        floodClaim(worldToChunkPos(this->settings.teamSpawns[i]), 50, i);
    for(int x = 0; x < this->settings.numChunks.x; x++)
    {
        for(int y = 0; y < this->settings.numChunks.y; y++)
        {
            auto& chunk = getChunk({x, y});
            if(chunk->getCurrentOwner() != -1) chunk->development = 1.f;
            updateTerritoryColor({x, y}, chunk);
        }
    }

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

            float targetSupply = distrib01(generator) > 0.5 ? 1.f : 3.f;

            auto c = std::make_shared<Cell>(teamId, seedDistrib(generator), 1, 1, 1, targetSupply,
                                            velocity, prefferedVelocity, position);
            sf::Vector2i chunkPos = worldToChunkPos(c->position);
            this->cells.push_back(c);
            auto& chunk = this->getChunk(chunkPos);
            chunk->cells[c->teamId].push_back(c);

            //if (chunk->teamOwnership[c->teamId] != 1.f)
            //{
            //    for (int k = 0; k < this->settings.numTeams; k++)
            //        chunk->teamOwnership[k] = k == c->teamId ? 1.f : 0.f;
            //}
        }
    }

    for (const auto& chunk: chunks)
    {
        bool isCity = distrib01(generator) > 0.98f;
        bool isMegapolis = isCity && distrib01(generator) > 0.99f;
        chunk->supplyGeneration = distrib01(generator) * 0.1f;
        if(isCity)
            chunk->supplyGeneration *= 10.f;
        if(isMegapolis)
            chunk->supplyGeneration *= 10.f;

        if(chunk->supplyGeneration > maxSupplyGeneration)
            maxSupplyGeneration = chunk->supplyGeneration;
    }
}

void World::step(float delta)
{
    delta *= settings.speed;

    this->worldTime += delta;

    updateTerritories(delta);
    developChunks(delta);
    updateChunkSupply(delta);
    updateCellSupply(delta);
    updateVelocities(delta);
    updatePositions(delta);
    attackNearby(delta);
    spawnChildren(delta);
}

void World::draw(sf::RenderTarget& target, sf::RenderStates states) const
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
    }
    else if (viewMode == ViewMode::SUPPLY)
    {
        sf::Image img = sf::Image();
        img.create(settings.numChunks.x, settings.numChunks.y);
        for (int x = 0; x < settings.numChunks.x; x++)
        {
            for (int y = 0; y < settings.numChunks.y; y++)
            {
                auto& chunk = getChunk(sf::Vector2i(x, y));
                sf::Vector3f colorVec = chunk->supply * sf::Vector3f(255.f, 255.f, 255.f) / (10.f * maxSupplyGeneration);
                img.setPixel(x, y,sf::Color((uint8_t) colorVec.x, (uint8_t) colorVec.y, (uint8_t) colorVec.z));
            }
        }

        sf::Texture t = sf::Texture();
        t.loadFromImage(img);
        sf::Sprite sprite(t);
        sprite.setScale(settings.pixelsPerChunk, settings.pixelsPerChunk);

        target.draw(sprite, states);
    }
    else if(viewMode == ViewMode::SUPPLY_GENERATION)
    {
        sf::Image img = sf::Image();
        img.create(settings.numChunks.x, settings.numChunks.y);
        for (int x = 0; x < settings.numChunks.x; x++)
        {
            for (int y = 0; y < settings.numChunks.y; y++)
            {
                auto& chunk = getChunk(sf::Vector2i(x, y));
                sf::Vector3f colorVec = chunk->getEffectiveSupplyGeneration() * sf::Vector3f(255.f, 255.f, 255.f) / maxSupplyGeneration;
                img.setPixel(x, y,sf::Color((uint8_t) colorVec.x, (uint8_t) colorVec.y, (uint8_t) colorVec.z));
            }
        }

        sf::Texture t = sf::Texture();
        t.loadFromImage(img);
        sf::Sprite sprite(t);
        sprite.setScale(settings.pixelsPerChunk, settings.pixelsPerChunk);

        target.draw(sprite, states);
    }
    // else impossible

    for (const auto& c: cells)
    {
        auto pos = c->position;
        circle.setPosition(pos);
        auto color = settings.teamColors[c->teamId];
        color.a = (uint8_t) lerp(150.f, 255.f, c->health);
        circle.setFillColor(color);
        target.draw(circle, states);
    }
}

World::~World() {}
