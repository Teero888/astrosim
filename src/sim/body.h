#ifndef BODY_H
#define BODY_H

#include "vmath.h"
#include <glm/gtc/type_ptr.hpp>
#include <string>

enum class EBodyType
{
	STAR,
	TERRESTRIAL,
	GAS_GIANT,
};

enum class ETerrainType
{
	TERRESTRIAL = 0,
	VOLCANIC = 1,
	ICE = 2,
	BARREN = 3,
};

struct STerrainParameters
{
	// Noise settings
	float ContinentFrequency = 0.02f;
	int ContinentOctaves = 4;
	float MountainFrequency = 0.08f;
	int MountainOctaves = 6;
	float HillsFrequency = 0.3f;
	int HillsOctaves = 3;
	float DetailFrequency = 0.8f;
	int DetailOctaves = 4;

	// Domain Warp
	float WarpFrequency = 0.05f; // Frequency of the warp noise
	float WarpStrength = 0.5f; // How much to distort the coordinates

	// Terrain shape
	float SeaLevel = 0.4f; // [0, 1] percentage of continent noise
	float OceanDepth = 0.01f; // % of PlanetRadius
	float ContinentHeight = 0.005f; // % of PlanetRadius
	float MountainHeight = 0.015f; // % of PlanetRadius
	float HillsHeight = 0.001f; // % of PlanetRadius
	float DetailHeight = 0.0001f; // % of PlanetRadius

	// Min *mountain component* (in meters) to be colored as a mountain
	float MountainStartMin = 1500.0f;
};

struct SColorPalette
{
	// Colors
	glm::vec3 DeepOcean = glm::vec3(0.0f, 0.1f, 0.3f);
	glm::vec3 ShallowOcean = glm::vec3(0.1f, 0.3f, 0.5f);
	glm::vec3 Beach = glm::vec3(0.85f, 0.75f, 0.5f);
	glm::vec3 LandLow = glm::vec3(0.2f, 0.5f, 0.2f);
	glm::vec3 LandHigh = glm::vec3(0.4f, 0.6f, 0.3f);
	glm::vec3 MountainLow = glm::vec3(0.4f, 0.3f, 0.2f);
	glm::vec3 MountainHigh = glm::vec3(0.3f, 0.2f, 0.15f);
	glm::vec3 Snow = glm::vec3(0.9f, 0.9f, 0.95f);

	// == TERRESTRIAL ==
	float OceanDepthMax = 2000.0f; // Depth for full DEEP_OCEAN color
	float BeachHeightMax = 50.0f; // Max elevation for BEACH color
	float LandHeightMax = 1000.0f; // Elevation for full LAND_HIGH color
	float MountainHeightMax = 5000.0f; // Elevation for full MOUNTAIN_HIGH color
	float SnowLineStart = 4500.0f; // Elevation where snow *starts*
	float SnowLineEnd = 6000.0f; // Elevation for full SNOW_COLOR

	// == VOLCANIC ==
	float LavaPoolHeight = 50.0f; // Max elevation for lava pools (uses ocean colors)
	float LavaRockHeight = 2000.0f; // Elevation for full LAND_HIGH (rock) color
	float LavaFlowStart = 0.6f; // Noise [0,1] for lava flow to start
	float LavaFlowMaskEnd = 1.0f; // Noise [0,1] for lava flow to be full
	float LavaPeakHeight = 4000.0f; // Elevation for full MOUNTAIN_HIGH (bright lava)
	float LavaHotspotHeight = 5000.0f; // Elevation for full SNOW (brightest hot) color

	// == ICE ==
	float SlushDepthMax = 1000.0f; // Depth for full DEEP_OCEAN (slush) color
	float IceSheetHeight = 200.0f; // Elevation for full LAND_HIGH (ice) color
	float CrevasseStart = 0.1f; // Noise [0,1] for crevasses to start
	float CrevasseMaskEnd = 1.0f; // Noise [0,1] for crevasses to be full
	float CrevasseColorMix = 0.7f; // [0,1] how much to mix crevasse color

	// == BARREN ==
	float BarrenLandMax = 2000.0f; // Elevation for full LAND_HIGH color
	float BarrenMountainMax = 3000.0f; // Elevation for full MOUNTAIN_HIGH color
};

struct SBody
{
	std::string m_Name;
	int m_Id;

	// clang-format off
	struct SSimParams
	{
		double m_Mass;			// Mass (kg)
		Vec3   m_Position;		// Position (m)
		Vec3   m_Velocity;		// Velocity (m/s)
		Vec3   m_Acceleration;	// Acceleration (m/s^2)
	} m_SimParams;

	struct SRenderParams
	{
		double m_Radius;	// Planetary radius [meters]
		glm::vec3 m_Color;	// Rgb color for stars or fallbacks
		EBodyType m_BodyType = EBodyType::TERRESTRIAL;
		ETerrainType m_TerrainType = ETerrainType::TERRESTRIAL;
		STerrainParameters m_Terrain;
		SColorPalette m_Colors;
		int m_Seed = 0;
	} m_RenderParams;
	// clang-format on

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H
