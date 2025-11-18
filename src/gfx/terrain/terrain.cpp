#include "terrain.h"
#include <FastNoiseLite.h>
#include <glm/gtc/noise.hpp>

CTerrainGenerator::CTerrainGenerator() :
	m_pContinentNoise(new FastNoiseLite()),
	m_pMountainNoise(new FastNoiseLite()),
	m_pHillsNoise(new FastNoiseLite()),
	m_pCaveNoise(new FastNoiseLite()),
	m_pWarpNoise(new FastNoiseLite())
{
}

CTerrainGenerator::~CTerrainGenerator()
{
	delete m_pContinentNoise;
	delete m_pMountainNoise;
	delete m_pHillsNoise;
	delete m_pCaveNoise;
	delete m_pWarpNoise;
}

void CTerrainGenerator::Init(int seed, const STerrainParameters &params, ETerrainType terrainType)
{
	m_TerrainType = terrainType;
	m_Params = params;

	// Domain Warp Noise (to distort coordinates)
	m_pWarpNoise->SetSeed(seed + 10);
	m_pWarpNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pWarpNoise->SetFrequency(m_Params.WarpFrequency);
	m_pWarpNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pWarpNoise->SetFractalOctaves(4);

	// Continent Noise (large scale features)
	m_pContinentNoise->SetSeed(seed);
	m_pContinentNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pContinentNoise->SetFrequency(params.ContinentFrequency);
	m_pContinentNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pContinentNoise->SetFractalOctaves(params.ContinentOctaves);
	m_pContinentNoise->SetFractalLacunarity(2.0f);
	m_pContinentNoise->SetFractalGain(0.5f);

	// Mountain Noise (mid scale features)
	m_pMountainNoise->SetSeed(seed + 1);
	m_pMountainNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pMountainNoise->SetFrequency(params.MountainFrequency);
	m_pMountainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pMountainNoise->SetFractalOctaves(params.MountainOctaves);
	m_pMountainNoise->SetFractalLacunarity(2.0f);
	m_pMountainNoise->SetFractalGain(0.5f);

	// Hills/Plains Noise (small scale detail)
	m_pHillsNoise->SetSeed(seed + 2);
	m_pHillsNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pHillsNoise->SetFrequency(params.HillsFrequency);
	// Using FBm for rolling hills is often more natural than Ridged for "plains"
	m_pHillsNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pHillsNoise->SetFractalOctaves(params.HillsOctaves);
	m_pHillsNoise->SetFractalLacunarity(2.0f);
	m_pHillsNoise->SetFractalGain(0.5f);

	// Cave Noise (3D internal features)
	m_pCaveNoise->SetSeed(seed + 3);
	m_pCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pCaveNoise->SetFrequency(0.1f); // Using default for now
	m_pCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pCaveNoise->SetFractalOctaves(3); // Using default for now
}

STerrainOutput CTerrainGenerator::GetTerrainOutput(glm::vec3 worldPosition, float planetRadius)
{
	float distance_from_center = glm::length(worldPosition);
	float base_density = planetRadius - distance_from_center;
	glm::vec3 normalized_pos = glm::normalize(worldPosition);

	// We use 3 noise lookups to create a 3D offset vector
	// We use offset inputs to the warp noise to avoid getting the same value
	float warp_x = m_pWarpNoise->GetNoise(normalized_pos.x, normalized_pos.y, normalized_pos.z);
	float warp_y = m_pWarpNoise->GetNoise(normalized_pos.y + 100.0f, normalized_pos.z, normalized_pos.x);
	float warp_z = m_pWarpNoise->GetNoise(normalized_pos.z, normalized_pos.x + 200.0f, normalized_pos.y);

	glm::vec3 warped_pos = normalized_pos + (glm::vec3(warp_x, warp_y, warp_z) * m_Params.WarpStrength);
	// We use the *warped* position to sample all our terrain noise
	// This creates natural swirls and breaks up the "grid" look
	warped_pos = glm::normalize(warped_pos); // Re-normalize to stay on sphere

	float continent_noise = m_pContinentNoise->GetNoise(warped_pos.x, warped_pos.y, warped_pos.z);
	float mountain_raw_noise = m_pMountainNoise->GetNoise(warped_pos.x * 4, warped_pos.y * 4, warped_pos.z * 4);
	float hills_raw_noise = m_pHillsNoise->GetNoise(warped_pos.x * 16, warped_pos.y * 16, warped_pos.z * 16);

	float terrain_offset = 0.0f;
	float elevation = 0.0f; // This will be the value for the shader
	float special_noise = 0.0f; // This will be the second value for the shader

	switch(m_TerrainType)
	{
	case ETerrainType::TERRESTRIAL:
	default:
	{
		// Terrestrial Logic
		float land_factor = (continent_noise + 1.0f) * 0.5f;
		land_factor = pow(land_factor, 1.5f);
		float continent_offset = 0;
		// Use parameters from m_Params
		if(land_factor > m_Params.SeaLevel)
			continent_offset = (land_factor - m_Params.SeaLevel) * (planetRadius * m_Params.ContinentHeight);
		else
			continent_offset = (land_factor - m_Params.SeaLevel) * (planetRadius * m_Params.OceanDepth);

		float mountain_noise = 0;
		float mountain_noise_raw_01 = 0.0f;
		if(land_factor > m_Params.SeaLevel)
		{
			mountain_noise_raw_01 = (mountain_raw_noise + 1.0f) * 0.5f; // to [0,1]
			mountain_noise_raw_01 = pow(mountain_noise_raw_01, 2.0f);
			// Scale mountains by how high the land is
			mountain_noise = mountain_noise_raw_01 * (planetRadius * m_Params.MountainHeight) * (land_factor - m_Params.SeaLevel);
		}

		// Modulate hills by mountain steepness.
		// Hills are flat (dampening = 1.0) on plains and flat (dampening = 0.0) on peaks.
		float hill_dampening = 1.0f - mountain_noise_raw_01;
		float hills_noise = hills_raw_noise * (planetRadius * m_Params.HillsHeight) * hill_dampening;

		terrain_offset = continent_offset + mountain_noise + hills_noise;

		// Set shader values
		elevation = terrain_offset;
		special_noise = mountain_noise; // Pass mountain height
		break;
	}
	case ETerrainType::BARREN:
	{
		// Barren Logic
		float continent_offset = continent_noise * (planetRadius * 0.005f);
		float m_raw = (mountain_raw_noise + 1.0f) * 0.5f;
		m_raw = pow(m_raw, 2.0f);
		float mountain_noise = m_raw * (planetRadius * 0.01f);
		float hills_noise = hills_raw_noise * (planetRadius * 0.001f);

		terrain_offset = continent_offset + mountain_noise + hills_noise;
		elevation = terrain_offset;
		special_noise = m_raw; // Pass raw mountain noise [0, 1]
		break;
	}
	case ETerrainType::VOLCANIC:
	{
		// Volcanic Logic
		float continent_offset = continent_noise * (planetRadius * 0.008f);
		float m_raw = (mountain_raw_noise + 1.0f) * 0.5f;
		m_raw = pow(m_raw, 3.0f); // Sharper peaks
		float mountain_noise = m_raw * (planetRadius * 0.02f);
		float hills_noise = hills_raw_noise * (planetRadius * 0.002f);

		terrain_offset = continent_offset + mountain_noise + hills_noise;
		elevation = terrain_offset;
		special_noise = m_raw; // Pass raw mountain noise [0, 1] for lava
		break;
	}
	case ETerrainType::ICE:
	{
		// Ice Logic
		float land_factor = (continent_noise + 1.0f) * 0.5f;
		float continent_offset = (land_factor - 0.9f) * (planetRadius * 0.01f);
		float crevasse_noise = 0;
		float m_raw = (mountain_raw_noise + 1.0f) * 0.5f; // to [0,1]
		if(m_raw > 0.7f) // Create sharp cracks
		{
			crevasse_noise = (m_raw - 0.7f) / 0.3f;
			special_noise = crevasse_noise; // Pass crevasse strength [0, 1]
			crevasse_noise = crevasse_noise * (planetRadius * -0.005f); // Carve down
		}
		float hills_noise = hills_raw_noise * (planetRadius * 0.0005f); // Very small bumps

		terrain_offset = continent_offset + crevasse_noise + hills_noise;
		elevation = terrain_offset;
		break;
	}
	}

	// 4. CAVE GENERATION
	float cave_effect = 0.0f;
	if(base_density + terrain_offset > 100.0f)
	{
		float cave_coord_scale = 1.0f / 10.0f;
		float cave_noise_val = m_pCaveNoise->GetNoise(worldPosition.x * cave_coord_scale,
			worldPosition.y * cave_coord_scale,
			worldPosition.z * cave_coord_scale);

		float cave_threshold = 0.7f;
		if(cave_noise_val > cave_threshold)
		{
			float remapped_noise = (cave_noise_val - cave_threshold) / (1.0f - cave_threshold);
			cave_effect = remapped_noise * 50.0f;
		}
	}

	// 5. FINAL COMBINATION
	float final_density = base_density + terrain_offset - cave_effect;

	// Return all our calculated data in one struct
	return {final_density, elevation, special_noise};
}

glm::vec3 CTerrainGenerator::CalculateDensityGradient(glm::vec3 p, float planetRadius)
{
	float eps = planetRadius * 0.0001f;
	if(eps < 1e-4f)
		eps = 1e-4f; // Avoid being too small on tiny planets

	float dx = GetTerrainOutput(p + glm::vec3(eps, 0, 0), planetRadius).density - GetTerrainOutput(p - glm::vec3(eps, 0, 0), planetRadius).density;
	float dy = GetTerrainOutput(p + glm::vec3(0, eps, 0), planetRadius).density - GetTerrainOutput(p - glm::vec3(0, eps, 0), planetRadius).density;
	float dz = GetTerrainOutput(p + glm::vec3(0, 0, eps), planetRadius).density - GetTerrainOutput(p - glm::vec3(0, 0, eps), planetRadius).density;

	// The gradient vector points from lower density to higher density.
	// For our SDF (density > 0 is inside), the gradient points inwards.
	// The surface normal should point outwards, so we negate the gradient.
	return -glm::normalize(glm::vec3(dx, dy, dz));
}
