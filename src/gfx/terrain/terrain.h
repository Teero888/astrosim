#ifndef TERRAIN_H
#define TERRAIN_H

#include "../../sim/body.h"
#include <glm/glm.hpp>

class FastNoiseLite;

struct STerrainColorData
{
	float elevation;
	float mountain_noise;
};

class CTerrainGenerator
{
public:
	CTerrainGenerator();
	~CTerrainGenerator();

	void Init(int seed, const STerrainParameters &params, ETerrainType terrainType);
	float GetTerrainDensity(glm::vec3 worldPosition, float planetRadius);
	glm::vec3 CalculateDensityGradient(glm::vec3 p, float planetRadius);
	STerrainColorData GetTerrainColorData(glm::vec3 worldPosition, float planetRadius);

private:
	FastNoiseLite *m_pContinentNoise;
	FastNoiseLite *m_pMountainNoise;
	FastNoiseLite *m_pHillsNoise;
	FastNoiseLite *m_pCaveNoise;
	ETerrainType m_TerrainType;
};

#endif // TERRAIN_H
