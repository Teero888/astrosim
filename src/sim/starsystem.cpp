#include "starsystem.h"
#include "../gfx/camera.h"
#include "../gfx/shader.h"
#include "body.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include <chrono>
#include <cstdio>

constexpr double G = 6.67430e-11; // gravitational constant

#include "vmath.h"

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_SimParams.m_Position - a.m_SimParams.m_Position; // Vector from a to b
	const double Distance = r.length();
	const double ForceMagnitude = (G * a.m_SimParams.m_Mass * b.m_SimParams.m_Mass) / (Distance * Distance); // F = G * m1 * m2 / r^2
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

static const std::chrono::time_point<std::chrono::steady_clock> StartTime = std::chrono::steady_clock::now();
void CStarSystem::RenderBody(SBody *pBody, SBody *pLightBody, CShader *pShader, class CCamera *pCamera)
{
	pShader->Use();

	// set transformation matrices
	glm::mat4 Model = glm::mat4(1.0f);
	Vec3 NewPos = (pBody->m_SimParams.m_Position - pCamera->m_pFocusedBody->m_SimParams.m_Position) / pCamera->m_Radius;
	Model = glm::translate(Model, (glm::vec3)NewPos);

	// ---------- shader props ----------

	pShader->SetFloat("Radius", pBody->m_RenderParams.m_Radius / pCamera->m_Radius);
	pShader->SetFloat("Roughness", pBody->m_RenderParams.m_Roughness);
	pShader->SetVec3("LightDir", pLightBody == pBody ? glm::vec3(0, 0, 0) : glm::normalize((glm::vec3)((pLightBody->m_SimParams.m_Position - pCamera->m_pFocusedBody->m_SimParams.m_Position) / pCamera->m_Radius)));
	pShader->SetVec3("CameraPos", glm::normalize(pCamera->m_Position));
	pShader->SetMat4("Model", Model);
	pShader->SetMat4("View", pCamera->m_View);
	pShader->SetMat4("Projection", pCamera->m_Projection);

	// bind the vao and draw the sphere
	glBindVertexArray(pShader->VAO);
	glDrawElements(GL_TRIANGLES, pShader->m_NumIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

// clang-format off
void CStarSystem::OnInit()
{
	int id = 0;
	m_vBodies = {
		SBody(id++, "Sun",
			{
				1.989e30, // Mass (kg)
				Vec3(0, 0, 0), // Position (m)
				Vec3(0, 0, 0), // Velocity (m/s)
				Vec3(0, 0, 0) // Acceleration
			},
			{
				6.957e8, // Radius (m)
				0.07f, // Albedo
				0.1f, // Roughness
				0.0f, // Specularity
				Vec3(1.0f, 0.93f, 0.85f), // Surface color (yellow-white)
				Vec3(0.0f), // Ocean color (none)
				Vec3(0.0f),
				Vec3(1.0f, 0.9f, 0.8f), // Atmosphere color (corona)
				0.2f, // Atmosphere strength
				0.0f, // Cloud cover
				15.0e6f, // Core temp (°C)
				5500.0f, // Surface temp (°C)
				0.0f, // Orbital speed (central star)
				glm::radians(360.0f / 27.0f) / 86400.0f, // Rotation speed (rad/s)
				0.0001f, // Magnetic field (T)
				0.0f // Aurora intensity
			}),
		SBody(id++, "Mercury",
			{
				3.3011e23,
				Vec3(5.791e10, 0, 0),
				Vec3(0, 0, 47870),
				Vec3(0, 0, 0)},
			{
				2.4397e6,
				0.12f, // Low albedo (Moon-like)
				0.85f, // Very rough surface
				0.05f, // Non-reflective
				Vec3(0.6f, 0.5f, 0.4f), // Gray-brown surface
				Vec3(0.0f), // No oceans
				Vec3(0.0f),
				Vec3(0.02f, 0.01f, 0.01f), // Trace exosphere
				0.01f, // Minimal atmosphere
				0.0f, // No clouds
				4300.0f, // Core temp
				167.0f, // Surface temp
				47870.0f, // Orbital speed
				glm::radians(360.0f / (58.6f * 24.0f)) / 3600.0f, // 58.6-day rotation
				0.0f, // Negligible magnetic field
				0.0f}),
		SBody(id++, "Venus",
			{
				4.8675e24,
				Vec3(1.082e11, 0, 0),
				Vec3(0, 0, 35020),
				Vec3(0, 0, 0)},
			{
				6.0518e6,
				0.75f, // High albedo (clouds)
				0.3f, // Smooth cloud layer
				0.9f, // Cloud reflectivity
				Vec3(0.9f, 0.7f, 0.3f), // Yellowish clouds
				Vec3(0.0f), // No liquid surface
				Vec3(0.0f),
				Vec3(0.8f, 0.6f, 0.4f), // Thick CO₂ atmosphere
				1.5f, // Dense atmosphere
				1.0f, // Full cloud cover
				4900.0f, // Core temp
				464.0f, // Surface temp
				35020.0f,
				glm::radians(-360.0f / (243.0f * 24.0f)) / 3600.0f, // Retrograde rotation
				0.0f,
				0.0f}),
		SBody(id++, "Earth",
			{
				5.972e24,
				Vec3(1.496e11, 0, 0),
				Vec3(0, 0, 29780),
				Vec3(0, 0, 0)},
			{
				6.371e6,
				0.3f, // Albedo
				0.2f, // Mixed roughness
				0.25f, // Ocean specularity
				Vec3(0.2f, 0.5f, 0.3f), // Vegetation/land color
				Vec3(0.0f, 0.15f, 0.35f), // Ocean blue
				Vec3(0.95f, 0.95f, 1.0f),
				Vec3(0.29f, 0.58f, 0.88f), // Atmospheric blue
				0.5f, // Moderate atmosphere
				0.6f, // Partial cloud cover
				6000.0f, // Core temp
				15.0f, // Surface temp
				29780.0f,
				glm::radians(360.0f) / 86400.0f, // 24-hour rotation
				50e-6f, // ~50μT magnetic field
				0.3f // Aurora intensity
			}),
		SBody(id++, "Moon",
			{
				7.342e22,
				Vec3(1.496e11 + 3.844e8, 0, 0),
				Vec3(0, 0, 29780 + 1022),
				Vec3(0, 0, 0)},
			{
				1.737e6,
				0.12f, // Low albedo
				0.9f, // Very rough
				0.02f, // Non-reflective
				Vec3(0.5f, 0.5f, 0.5f), // Gray regolith
				Vec3(0.0f), // No oceans
				Vec3(0.0f), 
				Vec3(0.0f), // No atmosphere
				0.0f,
				0.0f,
				1500.0f, // Core temp
				-23.0f, // Surface temp
				1022.0f,
				glm::radians(360.0f / (27.3f * 24.0f)) / 3600.0f, // Tidally locked
				0.0f,
				0.0f}),
		SBody(id++, "Jupiter",
			{
				1.8982e27,
				Vec3(7.785e11, 0, 0),
				Vec3(0, 0, 13070),
				Vec3(0, 0, 0)},
			{
				6.9911e7,
				0.52f, // Albedo
				0.4f, // Gaseous surface
				0.7f, // Cloud reflectivity
				Vec3(0.8f, 0.6f, 0.4f), // Band colors
				Vec3(0.1f, 0.1f, 0.5f), // Hypothetical liquid layer
				Vec3(0.9f, 0.9f, 1.0f), // idk
				Vec3(0.6f, 0.5f, 0.4f), // Upper atmosphere
				1.2f, // Thick atmosphere
				0.8f, // Cloud coverage
				24000.0f, // Core temp
				-145.0f, // Cloud top temp
				13070.0f,
				glm::radians(360.0f / 9.93f) / 3600.0f, // 9.9-hour rotation
				4.2e-4f, // Strong magnetic field
				0.8f // Intense auroras
			})};
}
// clang-format on

// static void PrintState(const std::vector<SBody> &vBodies, uint64_t Time)
// {
// 	printf("Time: %.3f\n", (double)Time / 24.0);
// 	for(const auto &body : vBodies)
// 		printf("%s - Pos: %.3f, %.3f, %.3f\n", body.m_Name.c_str(), body.m_Position.x, body.m_Position.y, body.m_Position.z);
// 	printf("\n");
// }

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

void CStarSystem::RenderBodies(class CShader *pShader, class CCamera *pCamera)
{
	for(auto &Body : m_vBodies)
		RenderBody(&Body, &m_vBodies.front(), pShader, pCamera);
}
