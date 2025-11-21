#ifndef BODY_H
#define BODY_H

#include "qmath.h" // Include the new Quat math
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
	float m_ContinentFrequency = 0.02f;
	int m_ContinentOctaves = 4;
	float m_ContinentHeight = 0.005f;
	float m_SeaLevel = 0.0f;
	float m_OceanDepth = 0.01f;

	float m_MountainFrequency = 0.08f;
	int m_MountainOctaves = 6;
	float m_MountainHeight = 0.015f;
	float m_MountainMaskFrequency = 0.01f;
	float m_MountainWarpStrength = 1.0f;

	float m_HillsFrequency = 0.3f;
	int m_HillsOctaves = 3;
	float m_HillsHeight = 0.001f;
	float m_DetailFrequency = 1.5f;
	int m_DetailOctaves = 4;
	float m_DetailHeight = 0.0005f;

	float m_PolarIceCapLatitude = 0.75f;
	float m_MoistureOffset = 0.0f;
	float m_TemperatureOffset = 0.0f;
};

struct SAtmosphereParameters
{
	bool m_Enabled = false;
	float m_AtmosphereRadius = 1.025f;
	float m_SunIntensity = 20.0f;

	glm::vec3 m_RayleighScatteringCoeff = glm::vec3(5.802e-6, 1.3558e-5, 3.31e-5);
	float m_RayleighScaleHeight = 8e3;

	glm::vec3 m_MieScatteringCoeff = glm::vec3(3.996e-6);
	float m_MieScaleHeight = 1.2e3;
	float m_MiePreferredScatteringDir = 0.758f;
};

struct SColorPalette
{
	glm::vec3 m_DeepOcean = glm::vec3(0.0f, 0.05f, 0.2f);
	glm::vec3 m_ShallowOcean = glm::vec3(0.0f, 0.2f, 0.4f);
	glm::vec3 m_Beach = glm::vec3(0.76f, 0.7f, 0.5f);

	glm::vec3 m_Grass = glm::vec3(0.15f, 0.35f, 0.1f);
	glm::vec3 m_Forest = glm::vec3(0.05f, 0.15f, 0.05f);
	glm::vec3 m_Desert = glm::vec3(0.8f, 0.6f, 0.4f);
	glm::vec3 m_Snow = glm::vec3(0.9f, 0.9f, 0.95f);
	glm::vec3 m_Rock = glm::vec3(0.25f, 0.25f, 0.25f);
	glm::vec3 m_Tundra = glm::vec3(0.35f, 0.35f, 0.3f);
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

		// == Rotational Physics ==
		// Current orientation quaternion
		Quat m_Orientation = Quat::Identity();
		// Angular velocity vector (axis * radians_per_sec)
		Vec3 m_AngularVelocity = Vec3(0.0);
	} m_SimParams;

	struct SRenderParams
	{
		double m_Radius;
		glm::vec3 m_Color;
		EBodyType m_BodyType = EBodyType::TERRESTRIAL;
		ETerrainType m_TerrainType = ETerrainType::TERRESTRIAL;
		STerrainParameters m_Terrain;
		SColorPalette m_Colors;
		SAtmosphereParameters m_Atmosphere;
		int m_Seed = 0;

		// == Rotational Config ==
		// Period in seconds (e.g. Earth = 86164.0)
		double m_RotationPeriod = 0.0;
		// Axial tilt in degrees (e.g. Earth = 23.5)
		double m_Obliquity = 0.0;
	} m_RenderParams;

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H