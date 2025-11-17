#include "terrain.h"
#include <FastNoiseLite.h>
#include <cmath>
#include <glm/gtc/noise.hpp>

CTerrainGenerator::CTerrainGenerator() :
	m_pContinentNoise(new FastNoiseLite()),
	m_pMountainNoise(new FastNoiseLite()),
	m_pHillsNoise(new FastNoiseLite()),
	m_pCaveNoise(new FastNoiseLite())
{
}

CTerrainGenerator::~CTerrainGenerator()
{
	delete m_pContinentNoise;
	delete m_pMountainNoise;
	delete m_pHillsNoise;
	delete m_pCaveNoise;
}

void CTerrainGenerator::Init(int seed, const STerrainParameters &params)
{
	// Continent Noise (large scale features)
	m_pContinentNoise->SetSeed(seed);
	m_pContinentNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pContinentNoise->SetFrequency(params.continentFrequency);
	m_pContinentNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pContinentNoise->SetFractalOctaves(params.continentOctaves);
	m_pContinentNoise->SetFractalLacunarity(2.0f);
	m_pContinentNoise->SetFractalGain(0.5f);

	// Mountain Noise (mid scale features)
	m_pMountainNoise->SetSeed(seed + 1);
	m_pMountainNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pMountainNoise->SetFrequency(params.mountainFrequency);
	m_pMountainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pMountainNoise->SetFractalOctaves(params.mountainOctaves);
	m_pMountainNoise->SetFractalLacunarity(2.0f);
	m_pMountainNoise->SetFractalGain(0.5f);

	// Hills/Plains Noise (small scale detail)
	m_pHillsNoise->SetSeed(seed + 2);
	m_pHillsNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pHillsNoise->SetFrequency(params.hillsFrequency);
	m_pHillsNoise->SetFractalType(FastNoiseLite::FractalType_Ridged); // Ridged gives more interesting plains
	m_pHillsNoise->SetFractalOctaves(params.hillsOctaves);
	m_pHillsNoise->SetFractalLacunarity(2.0f);
	m_pHillsNoise->SetFractalGain(0.5f);

	// Cave Noise (3D internal features)
	m_pCaveNoise->SetSeed(seed + 3);
	m_pCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pCaveNoise->SetFrequency(0.1f); // Using default for now
	m_pCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pCaveNoise->SetFractalOctaves(3); // Using default for now
}

float CTerrainGenerator::GetTerrainDensity(glm::vec3 worldPosition, float planetRadius)
{
	float distance_from_center = glm::length(worldPosition);
	float base_density = planetRadius - distance_from_center;

	// Normalize worldPosition for consistent noise sampling across planets of different sizes
	glm::vec3 normalized_pos = glm::normalize(worldPosition);

	// Continent Generation
	float continent_noise = m_pContinentNoise->GetNoise(normalized_pos.x, normalized_pos.y, normalized_pos.z);
	// Remap noise from [-1, 1] to [0, 1] to create landmasses and oceans
	float land_factor = (continent_noise + 1.0f) * 0.5f;
	// Sharpen the transition between land and sea
	land_factor = pow(land_factor, 1.5f);
	float continent_offset = 0;
	float sea_level = 0.4f;
	if(land_factor > sea_level)
		continent_offset = (land_factor - sea_level) * (planetRadius * 0.005f); // Continents are up to 0.5% of radius higher
	else
		continent_offset = (land_factor - sea_level) * (planetRadius * 0.01f); // Oceans are up to 1% of radius deeper

	// Mountain Generation (only on land)
	float mountain_noise = 0;
	if(land_factor > sea_level)
	{
		float mountain_raw_noise = m_pMountainNoise->GetNoise(normalized_pos.x * 4, normalized_pos.y * 4, normalized_pos.z * 4);
		// Make mountains more sparse by squaring the noise
		mountain_raw_noise = (mountain_raw_noise + 1.0f) * 0.5f; // to [0,1]
		mountain_raw_noise = pow(mountain_raw_noise, 2.0f);
		// Scale mountains by how high the land is, preventing mountains from rising from the ocean floor
		mountain_noise = mountain_raw_noise * (planetRadius * 0.015f) * (land_factor - sea_level);
	}

	// Hills and Plains
	float hills_raw_noise = m_pHillsNoise->GetNoise(normalized_pos.x * 16, normalized_pos.y * 16, normalized_pos.z * 16);
	float hills_noise = hills_raw_noise * (planetRadius * 0.001f); // Hills are small, 0.1% of radius

	// Total terrain offset
	float terrain_offset = continent_offset + mountain_noise + hills_noise;

	// Cave Generation (mostly underground)
	float cave_effect = 0.0f;
	// Apply caves only below the surface
	if(base_density + terrain_offset > 100.0f)
	{
		// Only carve caves > 100m below surface
		float cave_coord_scale = 1.0f / 10.0f;
		float cave_noise_val = m_pCaveNoise->GetNoise(worldPosition.x * cave_coord_scale,
			worldPosition.y * cave_coord_scale,
			worldPosition.z * cave_coord_scale);

		float cave_threshold = 0.7f;
		if(cave_noise_val > cave_threshold)
		{
			// Smoothly remap the noise value to create a clearer cave opening
			float remapped_noise = (cave_noise_val - cave_threshold) / (1.0f - cave_threshold);
			cave_effect = remapped_noise * 50.0f; // Caves are up to 50m "hollow"
		}
	}

	// Final Combination
	// Density = (Sphere SDF) + (Terrain Height) - (Cave Pockets)
	return base_density + terrain_offset - cave_effect;
}

STerrainColorData CTerrainGenerator::GetTerrainColorData(glm::vec3 worldPosition, float planetRadius)
{
	glm::vec3 normalized_pos = glm::normalize(worldPosition);

	// Re-calculate noise values needed for color
	float continent_noise = m_pContinentNoise->GetNoise(normalized_pos.x, normalized_pos.y, normalized_pos.z);
	float land_factor = (continent_noise + 1.0f) * 0.5f;
	land_factor = pow(land_factor, 1.5f);

	float sea_level = 0.4f;
	float continent_offset;
	if(land_factor > sea_level)
		continent_offset = (land_factor - sea_level) * (planetRadius * 0.005f);
	else
		continent_offset = (land_factor - sea_level) * (planetRadius * 0.01f);

	float mountain_noise = 0;
	if(land_factor > sea_level)
	{
		float mountain_raw_noise = m_pMountainNoise->GetNoise(normalized_pos.x * 4, normalized_pos.y * 4, normalized_pos.z * 4);
		mountain_raw_noise = (mountain_raw_noise + 1.0f) * 0.5f;
		mountain_raw_noise = pow(mountain_raw_noise, 2.0f);
		mountain_noise = mountain_raw_noise * (planetRadius * 0.015f) * (land_factor - sea_level);
	}

	float hills_raw_noise = m_pHillsNoise->GetNoise(normalized_pos.x * 16, normalized_pos.y * 16, normalized_pos.z * 16);
	float hills_noise = hills_raw_noise * (planetRadius * 0.001f);

	float elevation = continent_offset + mountain_noise + hills_noise;

	return {elevation, mountain_noise};
}

glm::vec3 CTerrainGenerator::CalculateDensityGradient(glm::vec3 p, float planetRadius)
{
	float eps = planetRadius * 0.0001f;
	if(eps < 1e-4f)
		eps = 1e-4f; // Avoid being too small on tiny planets

	float dx = GetTerrainDensity(p + glm::vec3(eps, 0, 0), planetRadius) - GetTerrainDensity(p - glm::vec3(eps, 0, 0), planetRadius);
	float dy = GetTerrainDensity(p + glm::vec3(0, eps, 0), planetRadius) - GetTerrainDensity(p - glm::vec3(0, eps, 0), planetRadius);
	float dz = GetTerrainDensity(p + glm::vec3(0, 0, eps), planetRadius) - GetTerrainDensity(p - glm::vec3(0, 0, eps), planetRadius);

	// The gradient vector points from lower density to higher density.
	// For our SDF (density > 0 is inside), the gradient points inwards.
	// The surface normal should point outwards, so we negate the gradient.
	return -glm::normalize(glm::vec3(dx, dy, dz));
}
