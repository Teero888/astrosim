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
	float ContinentFrequency = 0.02f;
	int ContinentOctaves = 4;
	float ContinentHeight = 0.005f;
	float SeaLevel = 0.0f;
	float OceanDepth = 0.01f;

	float MountainFrequency = 0.08f;
	int MountainOctaves = 6;
	float MountainHeight = 0.015f;
	float MountainMaskFrequency = 0.01f;
	float MountainWarpStrength = 1.0f;

	float HillsFrequency = 0.3f;
	int HillsOctaves = 3;
	float HillsHeight = 0.001f;
	float DetailFrequency = 1.5f;
	int DetailOctaves = 4;
	float DetailHeight = 0.0005f;

	float PolarIceCapLatitude = 0.75f;
	float MoistureOffset = 0.0f;
	float TemperatureOffset = 0.0f;
};

struct SAtmosphereParameters
{
	bool Enabled = false; // Replaces the old boolean in SRenderParams
	float AtmosphereRadius = 1.025f;
	float SunIntensity = 20.0f; // New customizable brightness

	glm::vec3 RayleighScatteringCoeff = glm::vec3(5.802e-6, 1.3558e-5, 3.31e-5);
	float RayleighScaleHeight = 8e3;

	glm::vec3 MieScatteringCoeff = glm::vec3(3.996e-6);
	float MieScaleHeight = 1.2e3;
	float MiePreferredScatteringDir = 0.758f;
};

struct SColorPalette
{
	glm::vec3 DeepOcean = glm::vec3(0.0f, 0.05f, 0.2f);
	glm::vec3 ShallowOcean = glm::vec3(0.0f, 0.2f, 0.4f);
	glm::vec3 Beach = glm::vec3(0.76f, 0.7f, 0.5f);

	glm::vec3 Grass = glm::vec3(0.15f, 0.35f, 0.1f);
	glm::vec3 Forest = glm::vec3(0.05f, 0.15f, 0.05f);
	glm::vec3 Desert = glm::vec3(0.8f, 0.6f, 0.4f);
	glm::vec3 Snow = glm::vec3(0.9f, 0.9f, 0.95f);
	glm::vec3 Rock = glm::vec3(0.25f, 0.25f, 0.25f);
	glm::vec3 Tundra = glm::vec3(0.35f, 0.35f, 0.3f);
};

struct SBody
{
	std::string m_Name;
	int m_Id;

	struct SSimParams
	{
		double m_Mass;
		Vec3 m_Position;
		Vec3 m_Velocity;
		Vec3 m_Acceleration;
	} m_SimParams;

	struct SRenderParams
	{
		double m_Radius;
		glm::vec3 m_Color;
		// bool m_HasAtmosphere = true; // REMOVED: Replaced by m_Atmosphere.Enabled
		EBodyType m_BodyType = EBodyType::TERRESTRIAL;
		ETerrainType m_TerrainType = ETerrainType::TERRESTRIAL;
		STerrainParameters m_Terrain;
		SColorPalette m_Colors;
		SAtmosphereParameters m_Atmosphere;
		int m_Seed = 0;
	} m_RenderParams;

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H
