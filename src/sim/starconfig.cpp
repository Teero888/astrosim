#include "../gfx/graphics.h"
#include "body.h"
#include "starsystem.h"

#include <string>

#include "glm/ext/vector_float3.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

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
			// Initialize Rotation Physics
			if(RenderParams.m_RotationPeriod > 0.0)
			{
				Vec3 orbitalUp(0, 1, 0);
				Quat tilt = Quat::FromAxisAngle(Vec3(0, 0, 1), -RenderParams.m_Obliquity); // Tilt 'right'
				Vec3 rotationAxis = tilt.RotateVector(orbitalUp);
				SimParams.m_Orientation = tilt;
				double angSpeed = (2.0 * PI) / RenderParams.m_RotationPeriod;
				SimParams.m_AngularVelocity = rotationAxis * angSpeed;
			}

			m_vBodies.emplace_back(IdCounter++, Name, SimParams, RenderParams);
			SimParams = {};
			RenderParams = {};
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
					RenderParams.m_Atmosphere.m_Enabled = false;
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
			else if(Key == "RotationPeriod")
				RenderParams.m_RotationPeriod = std::stod(value);
			else if(Key == "Obliquity")
				RenderParams.m_Obliquity = std::stod(value);
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
				RenderParams.m_Atmosphere.m_Enabled = (value == "true");
			}
		}
		else if(CurrentSelection == "colors")
		{
			glm::vec3 *pColor = nullptr;
			if(Key == "DeepOcean")
				pColor = &RenderParams.m_Colors.m_DeepOcean;
			else if(Key == "ShallowOcean")
				pColor = &RenderParams.m_Colors.m_ShallowOcean;
			else if(Key == "Beach")
				pColor = &RenderParams.m_Colors.m_Beach;
			else if(Key == "Grass")
				pColor = &RenderParams.m_Colors.m_Grass;
			else if(Key == "Forest")
				pColor = &RenderParams.m_Colors.m_Forest;
			else if(Key == "Desert")
				pColor = &RenderParams.m_Colors.m_Desert;
			else if(Key == "Snow")
				pColor = &RenderParams.m_Colors.m_Snow;
			else if(Key == "Rock")
				pColor = &RenderParams.m_Colors.m_Rock;
			else if(Key == "Tundra")
				pColor = &RenderParams.m_Colors.m_Tundra;

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
				RenderParams.m_Terrain.m_ContinentFrequency = std::stof(value);
			else if(Key == "ContinentOctaves")
				RenderParams.m_Terrain.m_ContinentOctaves = std::stoi(value);
			else if(Key == "MountainFrequency")
				RenderParams.m_Terrain.m_MountainFrequency = std::stof(value);
			else if(Key == "MountainOctaves")
				RenderParams.m_Terrain.m_MountainOctaves = std::stoi(value);
			else if(Key == "HillsFrequency")
				RenderParams.m_Terrain.m_HillsFrequency = std::stof(value);
			else if(Key == "HillsOctaves")
				RenderParams.m_Terrain.m_HillsOctaves = std::stoi(value);
			else if(Key == "DetailFrequency")
				RenderParams.m_Terrain.m_DetailFrequency = std::stof(value);
			else if(Key == "DetailOctaves")
				RenderParams.m_Terrain.m_DetailOctaves = std::stoi(value);
			else if(Key == "MountainWarpStrength")
				RenderParams.m_Terrain.m_MountainWarpStrength = std::stof(value);
			else if(Key == "SeaLevel")
				RenderParams.m_Terrain.m_SeaLevel = std::stof(value);
			else if(Key == "OceanDepth")
				RenderParams.m_Terrain.m_OceanDepth = std::stof(value);
			else if(Key == "ContinentHeight")
				RenderParams.m_Terrain.m_ContinentHeight = std::stof(value);
			else if(Key == "MountainHeight")
				RenderParams.m_Terrain.m_MountainHeight = std::stof(value);
			else if(Key == "HillsHeight")
				RenderParams.m_Terrain.m_HillsHeight = std::stof(value);
			else if(Key == "DetailHeight")
				RenderParams.m_Terrain.m_DetailHeight = std::stof(value);
			else if(Key == "MountainMaskFrequency")
				RenderParams.m_Terrain.m_MountainMaskFrequency = std::stof(value);
			else if(Key == "PolarIceCapLatitude")
				RenderParams.m_Terrain.m_PolarIceCapLatitude = std::stof(value);
			else if(Key == "MoistureOffset")
				RenderParams.m_Terrain.m_MoistureOffset = std::stof(value);
			else if(Key == "TemperatureOffset")
				RenderParams.m_Terrain.m_TemperatureOffset = std::stof(value);
		}
		else if(CurrentSelection == "gasgiant")
		{
			if(Key == "BaseColor")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_GasGiant.m_BaseColor.x >> comma >> RenderParams.m_GasGiant.m_BaseColor.y >> comma >> RenderParams.m_GasGiant.m_BaseColor.z;
			}
			else if(Key == "BandColor")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_GasGiant.m_BandColor.x >> comma >> RenderParams.m_GasGiant.m_BandColor.y >> comma >> RenderParams.m_GasGiant.m_BandColor.z;
			}
			else if(Key == "WindSpeed")
				RenderParams.m_GasGiant.m_WindSpeed = std::stof(value);
			else if(Key == "Turbulence")
				RenderParams.m_GasGiant.m_Turbulence = std::stof(value);
			else if(Key == "Seed")
				RenderParams.m_GasGiant.m_Seed = std::stof(value);
		}
		else if(CurrentSelection == "atmosphere")
		{
			// Enabling atmosphere explicitly
			RenderParams.m_Atmosphere.m_Enabled = true;

			if(Key == "AtmosphereRadius")
				RenderParams.m_Atmosphere.m_AtmosphereRadius = std::stof(value);
			else if(Key == "SunIntensity")
				RenderParams.m_Atmosphere.m_SunIntensity = std::stof(value);
			else if(Key == "RayleighScatteringCoeff")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_Atmosphere.m_RayleighScatteringCoeff.x >> comma >> RenderParams.m_Atmosphere.m_RayleighScatteringCoeff.y >> comma >> RenderParams.m_Atmosphere.m_RayleighScatteringCoeff.z;
			}
			else if(Key == "RayleighScaleHeight")
				RenderParams.m_Atmosphere.m_RayleighScaleHeight = std::stof(value);
			else if(Key == "MieScatteringCoeff")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> RenderParams.m_Atmosphere.m_MieScatteringCoeff.x >> comma >> RenderParams.m_Atmosphere.m_MieScatteringCoeff.y >> comma >> RenderParams.m_Atmosphere.m_MieScatteringCoeff.z;
			}
			else if(Key == "MieScaleHeight")
				RenderParams.m_Atmosphere.m_MieScaleHeight = std::stof(value);
			else if(Key == "MiePreferredScatteringDir")
				RenderParams.m_Atmosphere.m_MiePreferredScatteringDir = std::stof(value);
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
