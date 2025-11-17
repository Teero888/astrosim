#ifndef TERRAIN_H
#define TERRAIN_H

#include <glm/glm.hpp>

class FastNoiseLite;

class CTerrainGenerator
{
public:
	CTerrainGenerator();
	~CTerrainGenerator();

	void Init(int seed);
	float GetTerrainDensity(glm::vec3 worldPosition, float planetRadius);
	glm::vec3 CalculateDensityGradient(glm::vec3 p, float planetRadius);

private:
	FastNoiseLite *m_pContinentNoise;
	FastNoiseLite *m_pMountainNoise;
	FastNoiseLite *m_pHillsNoise;
	FastNoiseLite *m_pCaveNoise;
};

#endif // TERRAIN_H
