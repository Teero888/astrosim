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

	void Init(int seed, const STerrainParameters &params);
	float GetTerrainDensity(glm::vec3 worldPosition, float planetRadius);
	glm::vec3 CalculateDensityGradient(glm::vec3 p, float planetRadius);
	STerrainColorData GetTerrainColorData(glm::vec3 worldPosition, float planetRadius);

private:
	FastNoiseLite *m_pContinentNoise;
	FastNoiseLite *m_pMountainNoise;
	FastNoiseLite *m_pHillsNoise;
	FastNoiseLite *m_pCaveNoise;
};

#endif // TERRAIN_H
