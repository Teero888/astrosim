#include "starsystem.h"
#include "../gfx/camera.h"
#include "../gfx/graphics.h"
#include "../gfx/shader.h"
#include "body.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/vector_float3.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

constexpr double G = 6.67430e-11; // gravitational constant

#include "vmath.h"

static const std::chrono::time_point<std::chrono::steady_clock> StartTime = std::chrono::steady_clock::now();


void CStarSystem::OnInit()
{
	std::ifstream file("data/bodies.dat");
	if (!file.is_open()) {
		// A bit of a hack, but it's better than crashing
		// TODO: proper error handling
		m_vBodies.emplace_back(0, "Error", SBody::SSimParams{1, Vec3(0,0,0), Vec3(0,0,0), Vec3(0,0,0)}, SBody::SRenderParams{1, glm::vec3(1,0,0)});
		return;
	}

	int id = 0;
	std::string line;
	
	std::string name;
	double mass = 0;
	double radius = 0;
	Vec3 position(0,0,0);
	Vec3 velocity(0,0,0);
	glm::vec3 color(1,1,1);

	auto reset_vars = [&]() {
		name = "";
		mass = 0;
		radius = 0;
		position = Vec3(0,0,0);
		velocity = Vec3(0,0,0);
		color = glm::vec3(1,1,1);
	};

	auto add_body = [&]() {
		if (!name.empty()) {
			m_vBodies.emplace_back(
				id++, name,
				SBody::SSimParams{mass, position, velocity, Vec3(0,0,0)},
				SBody::SRenderParams{radius, color}
			);
			reset_vars();
		}
	};

	while (std::getline(file, line))
	{
		if (line.empty())
		{
			add_body();
			continue;
        }

		std::stringstream ss(line);
		std::string key, value;
		std::getline(ss, key, '=');
		std::getline(ss, value);

		if (key == "name") {
			name = value;
		} else if (key == "mass") {
			mass = std::stod(value);
		} else if (key == "radius") {
			radius = std::stod(value);
		} else if (key == "position") {
			std::stringstream vec_ss(value);
			char comma;
			vec_ss >> position.x >> comma >> position.y >> comma >> position.z;
		} else if (key == "velocity") {
			std::stringstream vec_ss(value);
			char comma;
			vec_ss >> velocity.x >> comma >> velocity.y >> comma >> velocity.z;
		} else if (key == "color") {
			std::stringstream vec_ss(value);
			char comma;
			vec_ss >> color.x >> comma >> color.y >> comma >> color.z;
		}
	}
	add_body(); // add last body
}

// static void PrintState(const std::vector<SBody> &vBodies, uint64_t Time)
// {
// 	printf("Time: %.3f\n", (double)Time / 24.0);
// 	for(const auto &body : vBodies)
// 		printf("%s - Pos: %.3f, %.3f, %.3f\n", body.m_Name.c_str(), body.m_Position.x, body.m_Position.y, body.m_Position.z);
// 	printf("\n");
// }

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_SimParams.m_Position - a.m_SimParams.m_Position; // Vector from a to b
	const double Distance = r.length();
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
