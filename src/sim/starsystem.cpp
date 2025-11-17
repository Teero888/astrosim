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
static std::string trim(const std::string &str)
{
	size_t first = str.find_first_not_of(" \t\n\r");
	if(std::string::npos == first)
	{
		return str;
	}
	size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, (last - first + 1));
}

void CStarSystem::OnInit()
{
	LoadBodies("data/bodies.dat");
}

void CStarSystem::LoadBodies(const std::string &filename)
{
	m_vBodies.clear();

	std::ifstream file(filename);
	if(!file.is_open())
	{
		// A bit of a hack, but it's better than crashing
		// TODO: proper error handling
		std::cerr << "Error: Could not open bodies data file: " << filename << std::endl;
		m_vBodies.emplace_back(0, "Error", SBody::SSimParams{1, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0)}, SBody::SRenderParams{1, glm::vec3(1, 0, 0)});
		return;
	}

	int id_counter = 0;
	std::string line;

	SBody::SSimParams sim_params = {};
	SBody::SRenderParams render_params = {};
	std::string name = "";
	std::string current_section = "";

	auto add_body = [&]() {
		if(!name.empty())
		{
			m_vBodies.emplace_back(id_counter++, name, sim_params, render_params);
			// Reset for next body
			sim_params = {};
			render_params = {};
			name = "";
			current_section = "";
		}
	};

	while(std::getline(file, line))
	{
		line = trim(line);
		if(line.empty())
			continue;

		if(line[0] == '[' && line.back() == ']')
		{
			current_section = line.substr(1, line.length() - 2);
			continue;
		}

		std::stringstream ss(line);
		std::string key, value;
		std::getline(ss, key, '=');
		std::getline(ss, value);
		key = trim(key);
		value = trim(value);

		if(key == "Name")
		{
			add_body();
			current_section = "";
			name = value;
			continue; // Move to next line
		}

		// Skip if no body is being parsed yet
		if(name.empty())
			continue;

		if(current_section.empty()) // Base parameters
		{
			if(key == "Type")
			{
				if(value == "STAR")
					render_params.m_BodyType = EBodyType::STAR;
				else if(value == "TERRESTRIAL")
					render_params.m_BodyType = EBodyType::TERRESTRIAL;
				else if(value == "GAS_GIANT")
					render_params.m_BodyType = EBodyType::GAS_GIANT;
			}
			else if(key == "TerrainType")
			{
				// Convert string to enum
				if(value == "volcanic")
					render_params.m_TerrainType = ETerrainType::VOLCANIC;
				else if(value == "ice")
					render_params.m_TerrainType = ETerrainType::ICE;
				else if(value == "barren")
					render_params.m_TerrainType = ETerrainType::BARREN;
				else // Default
					render_params.m_TerrainType = ETerrainType::TERRESTRIAL;
			}
			else if(key == "Mass")
				sim_params.m_Mass = std::stod(value);
			else if(key == "Radius")
				render_params.m_Radius = std::stod(value);
			else if(key == "Position")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> sim_params.m_Position.x >> comma >> sim_params.m_Position.y >> comma >> sim_params.m_Position.z;
			}
			else if(key == "Velocity")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> sim_params.m_Velocity.x >> comma >> sim_params.m_Velocity.y >> comma >> sim_params.m_Velocity.z;
			}
			else if(key == "Color")
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> render_params.m_Color.x >> comma >> render_params.m_Color.y >> comma >> render_params.m_Color.z;
			}
		}
		else if(current_section == "colors")
		{
			glm::vec3 *color_ptr = nullptr;
			if(key == "DeepOcean")
				color_ptr = &render_params.m_Colors.deepOcean;
			else if(key == "ShallowOcean")
				color_ptr = &render_params.m_Colors.shallowOcean;
			else if(key == "Beach")
				color_ptr = &render_params.m_Colors.beach;
			else if(key == "LandLow")
				color_ptr = &render_params.m_Colors.landLow;
			else if(key == "LandHigh")
				color_ptr = &render_params.m_Colors.landHigh;
			else if(key == "MountainLow")
				color_ptr = &render_params.m_Colors.mountainLow;
			else if(key == "MountainHigh")
				color_ptr = &render_params.m_Colors.mountainHigh;
			else if(key == "Snow")
				color_ptr = &render_params.m_Colors.snow;

			if(color_ptr)
			{
				std::stringstream ssv(value);
				char comma;
				ssv >> color_ptr->x >> comma >> color_ptr->y >> comma >> color_ptr->z;
			}
		}
		else if(current_section == "terrain")
		{
			if(key == "ContinentFrequency")
				render_params.m_Terrain.continentFrequency = std::stof(value);
			else if(key == "ContinentOctaves")
				render_params.m_Terrain.continentOctaves = std::stoi(value);
			else if(key == "MountainFrequency")
				render_params.m_Terrain.mountainFrequency = std::stof(value);
			else if(key == "MountainOctaves")
				render_params.m_Terrain.mountainOctaves = std::stoi(value);
			else if(key == "HillsFrequency")
				render_params.m_Terrain.hillsFrequency = std::stof(value);
			else if(key == "HillsOctaves")
				render_params.m_Terrain.hillsOctaves = std::stoi(value);
		}
	}
	add_body(); // add the last body

	if(!m_vBodies.empty())
	{
		// Find the sun, don't just assume it's the front
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
	{
		m_pSunBody = nullptr; // No bodies loaded
	}
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
