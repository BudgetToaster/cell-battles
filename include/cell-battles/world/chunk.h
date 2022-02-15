#ifndef CELL_BATTLES_CHUNK_H
#define CELL_BATTLES_CHUNK_H

#include <vector>
#include <memory>
#include "cell.h"

class Chunk
{
    friend class World;

    std::vector<std::vector<std::shared_ptr<Cell>>> cells;
    int numTeams;
    std::vector<float> teamOwnership;
    std::vector<bool> claimable;
    bool isASpawn;


public:
    explicit Chunk(int numTeams, bool isASpawn);

    Chunk(const Chunk &) = delete;

    // Returns the teamId of the team who fully owns the chunk. -1 if not fully owned by any team.
    int getCurrentOwner();
};


#endif //CELL_BATTLES_CHUNK_H
