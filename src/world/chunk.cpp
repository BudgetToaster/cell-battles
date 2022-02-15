#include "world/chunk.h"

Chunk::Chunk(int numTeams, bool isASpawn) :
        cells(numTeams), numTeams(numTeams), teamOwnership(numTeams),
        claimable(numTeams), isASpawn(isASpawn)
{

}


int Chunk::getCurrentOwner()
{
    for (int i = 0; i < numTeams; i++)
        if (teamOwnership[i] == 1.f) return i;
        else if (teamOwnership[i] != 0.f) return -1;
    return -1;
}