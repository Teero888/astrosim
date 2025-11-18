#include "starsystem.h"
#include "../gfx/graphics.h"
#include "body.h"
#include "glm/ext/vector_float3.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

constexpr double G = 6.67430e-11; // gravitational constant

#include "vmath.h"

// Helper to trim whitespace from a string
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
	LoadBodies("../data/bodies.dat");
}

void CStarSystem::LoadBodies(const std::string &Filename)
{
	m_vBodies.clear();

	std::ifstream File(Filename);
	if(!File.is_open())
	{
		// A bit of a hack, but it's better than crashing
		// TODO: proper error handling
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
			// Reset for next body
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
		if(Line[0] == '#') // comments
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
			continue; // Move to next line
		}

		// Skip if no body is being parsed yet
		if(Name.empty())
			continue;

		if(CurrentSelection.empty()) // Base parameters
		{
			if(Key == "Type")
			{
				if(value == "STAR")
					RenderParams.m_BodyType = EBodyType::STAR;
				else if(value == "TERRESTRIAL")
					RenderParams.m_BodyType = EBodyType::TERRESTRIAL;
				else if(value == "GAS_GIANT")
					RenderParams.m_BodyType = EBodyType::GAS_GIANT;
			}
			else if(Key == "TerrainType")
			{
				// Convert string to enum
				if(value == "volcanic")
					RenderParams.m_TerrainType = ETerrainType::VOLCANIC;
				else if(value == "ice")
					RenderParams.m_TerrainType = ETerrainType::ICE;
				else if(value == "barren")
					RenderParams.m_TerrainType = ETerrainType::BARREN;
				else // Default
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
			else if(Key == "LandLow")
				pColor = &RenderParams.m_Colors.LandLow;
			else if(Key == "LandHigh")
				pColor = &RenderParams.m_Colors.LandHigh;
			else if(Key == "MountainLow")
				pColor = &RenderParams.m_Colors.MountainLow;
			else if(Key == "MountainHigh")
				pColor = &RenderParams.m_Colors.MountainHigh;
			else if(Key == "Snow")
				pColor = &RenderParams.m_Colors.Snow;

			if(pColor)
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> pColor->x >> comma >> pColor->y >> comma >> pColor->z;
			}
			// TERRESTRIAL
			else if(Key == "OceanDepthMax")
				RenderParams.m_Colors.OceanDepthMax = std::stof(value);
			else if(Key == "BeachHeightMax")
				RenderParams.m_Colors.BeachHeightMax = std::stof(value);
			else if(Key == "LandHeightMax")
				RenderParams.m_Colors.LandHeightMax = std::stof(value);
			else if(Key == "MountainHeightMax")
				RenderParams.m_Colors.MountainHeightMax = std::stof(value);
			else if(Key == "SnowLineStart")
				RenderParams.m_Colors.SnowLineStart = std::stof(value);
			else if(Key == "SnowLineEnd")
				RenderParams.m_Colors.SnowLineEnd = std::stof(value);
			// VOLCANIC
			else if(Key == "LavaPoolHeight")
				RenderParams.m_Colors.LavaPoolHeight = std::stof(value);
			else if(Key == "LavaRockHeight")
				RenderParams.m_Colors.LavaRockHeight = std::stof(value);
			else if(Key == "LavaFlowStart")
				RenderParams.m_Colors.LavaFlowStart = std::stof(value);
			else if(Key == "LavaFlowMaskEnd")
				RenderParams.m_Colors.LavaFlowMaskEnd = std::stof(value);
			else if(Key == "LavaPeakHeight")
				RenderParams.m_Colors.LavaPeakHeight = std::stof(value);
			else if(Key == "LavaHotspotHeight")
				RenderParams.m_Colors.LavaHotspotHeight = std::stof(value);
			// ICE
			else if(Key == "SlushDepthMax")
				RenderParams.m_Colors.SlushDepthMax = std::stof(value);
			else if(Key == "IceSheetHeight")
				RenderParams.m_Colors.IceSheetHeight = std::stof(value);
			else if(Key == "CrevasseStart")
				RenderParams.m_Colors.CrevasseStart = std::stof(value);
			else if(Key == "CrevasseMaskEnd")
				RenderParams.m_Colors.CrevasseMaskEnd = std::stof(value);
			else if(Key == "CrevasseColorMix")
				RenderParams.m_Colors.CrevasseColorMix = std::stof(value);
			// BARREN
			else if(Key == "BarrenLandMax")
				RenderParams.m_Colors.BarrenLandMax = std::stof(value);
			else if(Key == "BarrenMountainMax")
				RenderParams.m_Colors.BarrenMountainMax = std::stof(value);
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
			// Load new parameters
			else if(Key == "WarpFrequency")
				RenderParams.m_Terrain.WarpFrequency = std::stof(value);
			else if(Key == "WarpStrength")
				RenderParams.m_Terrain.WarpStrength = std::stof(value);
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
			// Add parser for the new mountain-start value
			else if(Key == "MountainStartMin")
				RenderParams.m_Terrain.MountainStartMin = std::stof(value);
		}
	}
	AddBody(); // add the last body

	if(!m_vBodies.empty())
	{
		// Find the sun
		m_pSunBody = nullptr;
		for(auto &body : m_vBodies)
		{
			if(body.m_RenderParams.m_BodyType == EBodyType::STAR)
			{
				m_pSunBody = &body;
				break;
			}
		}
		// Fallback if no star
		if(!m_pSunBody)
			m_pSunBody = &m_vBodies.front();
	}
	else
		m_pSunBody = nullptr; // No bodies loaded
}

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_SimParams.m_Position - a.m_SimParams.m_Position; // Vector from a to b
	const double Distance = r.length();
	if(Distance < 1e-3)
		return Vec3(0); // Avoid division by zero if bodies are too close
	const double ForceMagnitude = (G * a.m_SimParams.m_Mass * b.m_SimParams.m_Mass) / (Distance * Distance); // F = G * m1 * m2 / r^2
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

void CStarSystem::UpdateBodies()
{
	if(m_vBodies.empty())
		return;

	// update velocities by half a time step using current accelerations
	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);

	// update positions using the half-step updated velocities
	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Position = Body.m_SimParams.m_Position + Body.m_SimParams.m_Velocity * m_DeltaTime;

	// calculate new accelerations based on updated positions
	for(auto &Body : m_vBodies)
	{
		Vec3 TotalForce(0, 0, 0);
		for(const auto &other : m_vBodies)
			if(&Body != &other)
				TotalForce = TotalForce + CalculateForce(Body, other);
		Body.m_SimParams.m_Acceleration = TotalForce / Body.m_SimParams.m_Mass; // a = F / m
	}

	// update velocities by the other half time step using new accelerations
	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);

	++m_SimTick;
}
