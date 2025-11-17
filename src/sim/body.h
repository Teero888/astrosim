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
	// TODO: Add more parameters as needed
	float continentFrequency = 0.02f;
	int continentOctaves = 4;
	float mountainFrequency = 0.08f;
	int mountainOctaves = 6;
	float hillsFrequency = 0.3f;
	int hillsOctaves = 3;
};

struct SColorPalette
{
	glm::vec3 deepOcean = glm::vec3(0.0f, 0.1f, 0.3f);
	glm::vec3 shallowOcean = glm::vec3(0.1f, 0.3f, 0.5f);
	glm::vec3 beach = glm::vec3(0.85f, 0.75f, 0.5f);
	glm::vec3 landLow = glm::vec3(0.2f, 0.5f, 0.2f);
	glm::vec3 landHigh = glm::vec3(0.4f, 0.6f, 0.3f);
	glm::vec3 mountainLow = glm::vec3(0.4f, 0.3f, 0.2f);
	glm::vec3 mountainHigh = glm::vec3(0.3f, 0.2f, 0.15f);
	glm::vec3 snow = glm::vec3(0.9f, 0.9f, 0.95f);
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
	} m_RenderParams;
	// clang-format on

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H
