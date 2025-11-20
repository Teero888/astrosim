#ifndef TERRAIN_H
#define TERRAIN_H

#include "../../sim/body.h"
#include "../../sim/vmath.h" // Include Vec3
#include <glm/glm.hpp>

class FastNoiseLite;

struct STerrainOutput
{
	float density;
	float elevation;
	float temperature;
	float moisture;
	float material_mask;
};

class CTerrainGenerator
{
public:
	CTerrainGenerator();
	~CTerrainGenerator();

	void Init(int Seed, const STerrainParameters &Params, ETerrainType TerrainType);

	STerrainOutput GetTerrainOutput(Vec3 WorldPosition, double PlanetRadius);

	glm::vec3 CalculateDensityGradient(Vec3 p, double PlanetRadius);

private:
	FastNoiseLite *m_pContinentNoise;
	FastNoiseLite *m_pMountainNoise;
	FastNoiseLite *m_pHillsNoise;
	FastNoiseLite *m_pDetailNoise;
	FastNoiseLite *m_pWarpNoise;

	FastNoiseLite *m_pMountainMaskNoise;
	FastNoiseLite *m_pBiomeNoise;

	ETerrainType m_TerrainType;
	STerrainParameters m_Params;
};

#endif // TERRAIN_H
