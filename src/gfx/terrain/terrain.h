#ifndef TERRAIN_H
#define TERRAIN_H

#include "../../sim/body.h"
#include <glm/glm.hpp>

class FastNoiseLite;

struct STerrainOutput
{
	float density; // Final SDF value
	float elevation; // The terrain offset, for color
	float special_noise; // The second color data channel (e.g., mountain, lava)
};

class CTerrainGenerator
{
public:
	CTerrainGenerator();
	~CTerrainGenerator();

	void Init(int seed, const STerrainParameters &params, ETerrainType terrainType);

	// This is our new, all-in-one function.
	STerrainOutput GetTerrainOutput(glm::vec3 worldPosition, float planetRadius);

	// This function is now much more efficient as it calls GetTerrainOutput
	glm::vec3 CalculateDensityGradient(glm::vec3 p, float planetRadius);

private:
	FastNoiseLite *m_pContinentNoise;
	FastNoiseLite *m_pMountainNoise;
	FastNoiseLite *m_pHillsNoise;
	FastNoiseLite *m_pCaveNoise;
	FastNoiseLite *m_pWarpNoise;
	ETerrainType m_TerrainType;
	STerrainParameters m_Params; // Store params for use in GetTerrainOutput
};

#endif // TERRAIN_H
