
// Constants
#include "starsystem.h"
#include "body.h"
#include <cstdio>

constexpr double G = 6.67430e-11; // gravitational constant
// basically the speed/accuracy of the simultion
constexpr double dt = 60.0 * 60.0; // Time step (1 hour in seconds)

#include "vmath.h"

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_Position - a.m_Position; // Vector from a to b
	const double Distance = r.length();
	const double ForceMagnitude = (G * a.m_Mass * b.m_Mass) / (Distance * Distance); // F = G * m1 * m2 / r^2
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

void CStarSystem::OnInit()
{
	// add earth and sun for testing
	// we are assuming Sun is at the center kinda funny
	// stuff from https://space.fandom.com/wiki/List_of_solar_system_objects_by_mass
	m_vBodies = {
		SBody("Sun", 1.989e30, Vec3(0, 0, 0), Vec3(0, 0, 0)),

		// https://www.britannica.com/story/what-is-earths-velocity
		SBody("Earth", 5.972e24, Vec3(1.496e11, 0, 0), Vec3(0, 29780, 0)) // Earth at 1 AU from Sun
	};
}

// static void PrintState(const std::vector<SBody> &vBodies, uint64_t Time)
// {
// 	printf("Time: %.3f\n", (double)Time / 24.0);
// 	for(const auto &body : vBodies)
// 		printf("%s - Pos: %.3f, %.3f, %.3f\n", body.m_Name.c_str(), body.m_Position.x, body.m_Position.y, body.m_Position.z);
// 	printf("\n");
// }

void CStarSystem::UpdateBodies()
{
	// PrintState(m_vBodies, m_SimTick);

	// yea no xd
	if(m_vBodies.empty())
		return;

	// update positions first
	for(auto &Body : m_vBodies)
		Body.m_Position = Body.m_Position + Body.m_Velocity * dt + Body.m_Acceleration * (0.5 * dt * dt);

	// calculate new accelerations
	for(auto &Body : m_vBodies)
	{
		Vec3 TotalForce(0, 0, 0);
		for(const auto &other : m_vBodies)
			if(&Body != &other)
				TotalForce = TotalForce + CalculateForce(Body, other);
		Body.m_Acceleration = TotalForce / Body.m_Mass; // a = F / m
	}

	// update velocities afterwards to avoid breaking the ordering
	for(auto &Body : m_vBodies)
		Body.m_Velocity = Body.m_Velocity + Body.m_Acceleration * dt;

	++m_SimTick;
}

void CStarSystem::RenderBodies(class CShader *pShader, class CCamera *pCamera)
{
	for(auto &Body : m_vBodies)
		Body.Render(pShader, pCamera);
}
