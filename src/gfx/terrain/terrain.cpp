#include "terrain.h"
#include <FastNoiseLite.h>
#include <glm/gtc/noise.hpp>

CTerrainGenerator::CTerrainGenerator() :
	m_pContinentNoise(new FastNoiseLite()),
	m_pMountainNoise(new FastNoiseLite()),
	m_pHillsNoise(new FastNoiseLite()),
	m_pDetailNoise(new FastNoiseLite()),
	m_pWarpNoise(new FastNoiseLite()),
	m_pMountainMaskNoise(new FastNoiseLite()),
	m_pBiomeNoise(new FastNoiseLite())
{
}

CTerrainGenerator::~CTerrainGenerator()
{
	delete m_pContinentNoise;
	delete m_pMountainNoise;
	delete m_pHillsNoise;
	delete m_pDetailNoise;
	delete m_pWarpNoise;
	delete m_pMountainMaskNoise;
	delete m_pBiomeNoise;
}

void CTerrainGenerator::Init(int seed, const STerrainParameters &params, ETerrainType terrainType)
{
	m_TerrainType = terrainType;
	m_Params = params;

	m_pContinentNoise->SetSeed(seed);
	m_pContinentNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pContinentNoise->SetFrequency(params.m_ContinentFrequency);
	m_pContinentNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pContinentNoise->SetFractalOctaves(params.m_ContinentOctaves);

	m_pMountainMaskNoise->SetSeed(seed + 100);
	m_pMountainMaskNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pMountainMaskNoise->SetFrequency(params.m_MountainMaskFrequency);

	m_pMountainNoise->SetSeed(seed + 1);
	m_pMountainNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pMountainNoise->SetFrequency(params.m_MountainFrequency);
	m_pMountainNoise->SetFractalType(FastNoiseLite::FractalType_Ridged);
	m_pMountainNoise->SetFractalOctaves(params.m_MountainOctaves);

	m_pHillsNoise->SetSeed(seed + 2);
	m_pHillsNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pHillsNoise->SetFrequency(params.m_HillsFrequency);
	m_pHillsNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pHillsNoise->SetFractalOctaves(params.m_HillsOctaves);

	m_pDetailNoise->SetSeed(seed + 4);
	m_pDetailNoise->SetFrequency(params.m_DetailFrequency);
	m_pDetailNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pDetailNoise->SetFractalOctaves(params.m_DetailOctaves);

	m_pBiomeNoise->SetSeed(seed + 99);
	m_pBiomeNoise->SetFrequency(1.5f);
	m_pBiomeNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

	m_pWarpNoise->SetSeed(seed + 5);
	m_pWarpNoise->SetFrequency(1.0f);
}

STerrainOutput CTerrainGenerator::GetTerrainOutput(Vec3 worldPosition, double planetRadius)
{
	// 1. Use Doubles for Normalization
	double distance_from_center = worldPosition.length();
	float base_density = (float)(planetRadius - distance_from_center);

	Vec3 normDouble = worldPosition.normalize();

	// 2. Use Doubles for noise inputs to preserve 1:1 scale precision
	// (Assuming FastNoiseLite is configured for double, or relying on implicit cast if not)
	double nx = normDouble.x;
	double ny = normDouble.y;
	double nz = normDouble.z;

	float wx = m_pWarpNoise->GetNoise(nx, ny, nz);
	float wy = m_pWarpNoise->GetNoise(ny, nz, nx);
	float wz = m_pWarpNoise->GetNoise(nz, nx, ny);

	// Apply warp to double coordinates
	double w_nx = nx + (double)wx * 0.1;
	double w_ny = ny + (double)wy * 0.1;
	double w_nz = nz + (double)wz * 0.1;

	float continent_val = m_pContinentNoise->GetNoise(w_nx, w_ny, w_nz);

	float terrain_height = 0.0f;
	bool isLand = continent_val > m_Params.m_SeaLevel;

	if(isLand)
	{
		terrain_height += (continent_val) * ((float)planetRadius * m_Params.m_ContinentHeight);
	}
	else
	{
		terrain_height += (continent_val) * ((float)planetRadius * m_Params.m_OceanDepth);
	}

	float mountain_mask = m_pMountainMaskNoise->GetNoise(nx, ny, nz);
	float m_val = 0.0f;

	if(mountain_mask > 0.2f)
	{
		float mask_strength = (mountain_mask - 0.2f) / 0.8f;
		float m_raw = m_pMountainNoise->GetNoise(w_nx, w_ny, w_nz);

		m_val = 1.0f - std::abs(m_raw);
		m_val = pow(m_val, 3.0f);

		terrain_height += m_val * mask_strength * ((float)planetRadius * m_Params.m_MountainHeight);
	}

	float hill_mask = 1.0f - glm::clamp(m_val, 0.0f, 1.0f);
	float hills = m_pHillsNoise->GetNoise(w_nx, w_ny, w_nz);
	terrain_height += hills * hill_mask * ((float)planetRadius * m_Params.m_HillsHeight);

	float detail = m_pDetailNoise->GetNoise(w_nx * 4.0, w_ny * 4.0, w_nz * 4.0);
	terrain_height += detail * hill_mask * ((float)planetRadius * m_Params.m_DetailHeight);

	float latitude = std::abs((float)ny); // Latitude calculation is fine in float
	float ice_influence = 0.0f;
	if(latitude > m_Params.m_PolarIceCapLatitude)
	{
		float t = (latitude - m_Params.m_PolarIceCapLatitude) / (1.0f - m_Params.m_PolarIceCapLatitude);
		t = t * t;
		ice_influence = t;
		terrain_height += t * ((float)planetRadius * 0.002f);
	}

	float temp_noise = m_pBiomeNoise->GetNoise(nx + 50.0, ny, nz);
	float base_temp = 1.0f - latitude;
	float height_cooling = (terrain_height / (float)planetRadius) * 100.0f;

	float final_temp = base_temp + (temp_noise * 0.2f) - height_cooling + m_Params.m_TemperatureOffset;
	final_temp = glm::clamp(final_temp, 0.0f, 1.0f);

	float moisture_noise = m_pBiomeNoise->GetNoise(nx, ny + 50.0, nz);
	float final_moisture = (moisture_noise * 0.5f + 0.5f) + m_Params.m_MoistureOffset;
	if(terrain_height < 0.0f)
		final_moisture += 0.2f;
	final_moisture = glm::clamp(final_moisture, 0.0f, 1.0f);

	float material_mask = ice_influence;

	return {
		base_density + terrain_height,
		terrain_height,
		final_temp,
		final_moisture,
		material_mask};
}

glm::vec3 CTerrainGenerator::CalculateDensityGradient(Vec3 p, double planetRadius)
{
	double eps = planetRadius * 0.0001;
	if(eps < 1e-4)
		eps = 1e-4;

	float dx = GetTerrainOutput(p + Vec3(eps, 0, 0), planetRadius).density - GetTerrainOutput(p - Vec3(eps, 0, 0), planetRadius).density;
	float dy = GetTerrainOutput(p + Vec3(0, eps, 0), planetRadius).density - GetTerrainOutput(p - Vec3(0, eps, 0), planetRadius).density;
	float dz = GetTerrainOutput(p + Vec3(0, 0, eps), planetRadius).density - GetTerrainOutput(p - Vec3(0, 0, eps), planetRadius).density;

	return -glm::normalize(glm::vec3(dx, dy, dz));
}
