#include "starsystem.h"
#include "../gfx/graphics.h"
#include "body.h"
#include "glm/ext/vector_float3.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

constexpr double G = 6.67430e-11;

#include "vmath.h"

static std::string trim(const std::string &Str)
{
	size_t First = Str.find_first_not_of(" \t\n\r");
	if(std::string::npos == First)
	{
		return Str;
	}
	size_t Last = Str.find_last_not_of(" \t\n\r");
	return Str.substr(First, (Last - First + 1));
}

void CStarSystem::OnInit()
{
	*this = CStarSystem();
	LoadBodies("data/bodies.dat");
}

void CStarSystem::LoadBodies(const std::string &Filename)
{
	m_vBodies.clear();

	std::ifstream File(Filename);
	if(!File.is_open())
	{
		std::cerr << "Error: Could not open bodies data file: " << Filename << std::endl;
		m_vBodies.emplace_back(0, "Error", SBody::SSimParams{1, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0)}, SBody::SRenderParams{1, glm::vec3(1, 0, 0)});
		return;
	}

	int IdCounter = 0;
	std::string Line;

	SBody::SSimParams SimParams = {};
	SBody::SRenderParams RenderParams = {};
	std::string Name = "";
	std::string CurrentSelection = "";

	auto AddBody = [&]() {
		if(!Name.empty())
		{
			m_vBodies.emplace_back(IdCounter++, Name, SimParams, RenderParams);
			SimParams = {};
			RenderParams = {}; // Resets m_Atmosphere.Enabled to false by default
			Name = "";
			CurrentSelection = "";
		}
	};

	while(std::getline(File, Line))
	{
		Line = trim(Line);
		if(Line.empty())
			continue;
		if(Line[0] == '#')
			continue;

		if(Line[0] == '[' && Line.back() == ']')
		{
			CurrentSelection = Line.substr(1, Line.length() - 2);
			continue;
		}

		std::stringstream ss(Line);
		std::string Key, value;
		std::getline(ss, Key, '=');
		std::getline(ss, value);
		Key = trim(Key);
		value = trim(value);

		if(Key == "Name")
		{
			AddBody();
			CurrentSelection = "";
			Name = value;
			continue;
		}

		if(Name.empty())
			continue;

		if(CurrentSelection.empty())
		{
			if(Key == "Type")
			{
				if(value == "STAR")
				{
					RenderParams.m_BodyType = EBodyType::STAR;
					RenderParams.m_Atmosphere.Enabled = false;
				}
				else if(value == "TERRESTRIAL")
					RenderParams.m_BodyType = EBodyType::TERRESTRIAL;
				else if(value == "GAS_GIANT")
					RenderParams.m_BodyType = EBodyType::GAS_GIANT;
			}
			else if(Key == "TerrainType")
			{
				if(value == "volcanic")
					RenderParams.m_TerrainType = ETerrainType::VOLCANIC;
				else if(value == "ice")
					RenderParams.m_TerrainType = ETerrainType::ICE;
				else if(value == "barren")
					RenderParams.m_TerrainType = ETerrainType::BARREN;
				else
					RenderParams.m_TerrainType = ETerrainType::TERRESTRIAL;
			}
			else if(Key == "Mass")
				SimParams.m_Mass = std::stod(value);
			else if(Key == "Radius")
				RenderParams.m_Radius = std::stod(value);
			else if(Key == "Seed")
				RenderParams.m_Seed = std::stod(value);
			else if(Key == "Position")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> SimParams.m_Position.x >> comma >> SimParams.m_Position.y >> comma >> SimParams.m_Position.z;
			}
			else if(Key == "Velocity")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> SimParams.m_Velocity.x >> comma >> SimParams.m_Velocity.y >> comma >> SimParams.m_Velocity.z;
			}
			else if(Key == "Color")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_Color.x >> comma >> RenderParams.m_Color.y >> comma >> RenderParams.m_Color.z;
			}
			else if(Key == "HasAtmosphere")
			{
				RenderParams.m_Atmosphere.Enabled = (value == "true");
			}
		}
		else if(CurrentSelection == "colors")
		{
			glm::vec3 *pColor = nullptr;
			if(Key == "DeepOcean")
				pColor = &RenderParams.m_Colors.DeepOcean;
			else if(Key == "ShallowOcean")
				pColor = &RenderParams.m_Colors.ShallowOcean;
			else if(Key == "Beach")
				pColor = &RenderParams.m_Colors.Beach;
			else if(Key == "Grass")
				pColor = &RenderParams.m_Colors.Grass;
			else if(Key == "Forest")
				pColor = &RenderParams.m_Colors.Forest;
			else if(Key == "Desert")
				pColor = &RenderParams.m_Colors.Desert;
			else if(Key == "Snow")
				pColor = &RenderParams.m_Colors.Snow;
			else if(Key == "Rock")
				pColor = &RenderParams.m_Colors.Rock;
			else if(Key == "Tundra")
				pColor = &RenderParams.m_Colors.Tundra;

			if(pColor)
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> pColor->x >> comma >> pColor->y >> comma >> pColor->z;
			}
		}
		else if(CurrentSelection == "terrain")
		{
			if(Key == "ContinentFrequency")
				RenderParams.m_Terrain.ContinentFrequency = std::stof(value);
			else if(Key == "ContinentOctaves")
				RenderParams.m_Terrain.ContinentOctaves = std::stoi(value);
			else if(Key == "MountainFrequency")
				RenderParams.m_Terrain.MountainFrequency = std::stof(value);
			else if(Key == "MountainOctaves")
				RenderParams.m_Terrain.MountainOctaves = std::stoi(value);
			else if(Key == "HillsFrequency")
				RenderParams.m_Terrain.HillsFrequency = std::stof(value);
			else if(Key == "HillsOctaves")
				RenderParams.m_Terrain.HillsOctaves = std::stoi(value);
			else if(Key == "DetailFrequency")
				RenderParams.m_Terrain.DetailFrequency = std::stof(value);
			else if(Key == "DetailOctaves")
				RenderParams.m_Terrain.DetailOctaves = std::stoi(value);
			else if(Key == "MountainWarpStrength")
				RenderParams.m_Terrain.MountainWarpStrength = std::stof(value);
			else if(Key == "SeaLevel")
				RenderParams.m_Terrain.SeaLevel = std::stof(value);
			else if(Key == "OceanDepth")
				RenderParams.m_Terrain.OceanDepth = std::stof(value);
			else if(Key == "ContinentHeight")
				RenderParams.m_Terrain.ContinentHeight = std::stof(value);
			else if(Key == "MountainHeight")
				RenderParams.m_Terrain.MountainHeight = std::stof(value);
			else if(Key == "HillsHeight")
				RenderParams.m_Terrain.HillsHeight = std::stof(value);
			else if(Key == "DetailHeight")
				RenderParams.m_Terrain.DetailHeight = std::stof(value);
			else if(Key == "MountainMaskFrequency")
				RenderParams.m_Terrain.MountainMaskFrequency = std::stof(value);
			else if(Key == "PolarIceCapLatitude")
				RenderParams.m_Terrain.PolarIceCapLatitude = std::stof(value);
			else if(Key == "MoistureOffset")
				RenderParams.m_Terrain.MoistureOffset = std::stof(value);
			else if(Key == "TemperatureOffset")
				RenderParams.m_Terrain.TemperatureOffset = std::stof(value);
		}
		else if(CurrentSelection == "atmosphere")
		{
			// Enabling atmosphere explicitly
			RenderParams.m_Atmosphere.Enabled = true;

			if(Key == "AtmosphereRadius")
				RenderParams.m_Atmosphere.AtmosphereRadius = std::stof(value);
			else if(Key == "SunIntensity")
				RenderParams.m_Atmosphere.SunIntensity = std::stof(value);
			else if(Key == "RayleighScatteringCoeff")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_Atmosphere.RayleighScatteringCoeff.x >> comma >> RenderParams.m_Atmosphere.RayleighScatteringCoeff.y >> comma >> RenderParams.m_Atmosphere.RayleighScatteringCoeff.z;
			}
			else if(Key == "RayleighScaleHeight")
				RenderParams.m_Atmosphere.RayleighScaleHeight = std::stof(value);
			else if(Key == "MieScatteringCoeff")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_Atmosphere.MieScatteringCoeff.x >> comma >> RenderParams.m_Atmosphere.MieScatteringCoeff.y >> comma >> RenderParams.m_Atmosphere.MieScatteringCoeff.z;
			}
			else if(Key == "MieScaleHeight")
				RenderParams.m_Atmosphere.MieScaleHeight = std::stof(value);
			else if(Key == "MiePreferredScatteringDir")
				RenderParams.m_Atmosphere.MiePreferredScatteringDir = std::stof(value);
		}
	}
	AddBody();

	if(!m_vBodies.empty())
	{
		m_pSunBody = nullptr;
		for(auto &body : m_vBodies)
		{
			if(body.m_RenderParams.m_BodyType == EBodyType::STAR)
			{
				m_pSunBody = &body;
				break;
			}
		}
		if(!m_pSunBody)
			m_pSunBody = &m_vBodies.front();
	}
	else
		m_pSunBody = nullptr;
}

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_SimParams.m_Position - a.m_SimParams.m_Position;
	const double Distance = r.length();
	if(Distance < 1e-3)
		return Vec3(0);
	const double ForceMagnitude = (G * a.m_SimParams.m_Mass * b.m_SimParams.m_Mass) / (Distance * Distance);
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

void CStarSystem::UpdateBodies()
{
	if(m_vBodies.empty())
		return;

	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);

	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Position = Body.m_SimParams.m_Position + Body.m_SimParams.m_Velocity * m_DeltaTime;

	for(auto &Body : m_vBodies)
	{
		Vec3 TotalForce(0, 0, 0);
		for(const auto &other : m_vBodies)
			if(&Body != &other)
				TotalForce = TotalForce + CalculateForce(Body, other);
		Body.m_SimParams.m_Acceleration = TotalForce / Body.m_SimParams.m_Mass;
	}

	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);

	++m_SimTick;
}
