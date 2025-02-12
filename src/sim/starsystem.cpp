#include "starsystem.h"
#include "../gfx/camera.h"
#include "../gfx/graphics.h"
#include "../gfx/shader.h"
#include "body.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include <chrono>
#include <cstdio>

constexpr double G = 6.67430e-11; // gravitational constant

#include "vmath.h"

static const std::chrono::time_point<std::chrono::steady_clock> StartTime = std::chrono::steady_clock::now();
void CStarSystem::RenderBody(SBody *pBody, SBody *pLightBody, CCamera &Camera)
{
	const Vec3 Distance = (pBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) + (Vec3)Camera.m_Position * Camera.m_Radius;
	const float Radius = (pBody->m_RenderParams.m_Radius / Distance.length());
	m_BodyShader.SetFloat("Scale", Radius);

	const Vec3 NewPos = (pBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_Radius;
	glm::vec2 ScreenPos = WorldToScreenCoordinates(NewPos, glm::mat4(1.f), Camera.m_View, Camera.m_Projection, Camera.m_ScreenSize.x, Camera.m_ScreenSize.y);
	if(ScreenPos.x < 0 || ScreenPos.y < 0 ||
		ScreenPos.x > Camera.m_ScreenSize.x || ScreenPos.y > Camera.m_ScreenSize.y)
		return;

	glm::vec2 Pos;
	Pos.x = (ScreenPos.x / Camera.m_ScreenSize.x) * 2.0f - 1.0f; // converts x to [-1, 1]
	Pos.y = 1.0f - (ScreenPos.y / Camera.m_ScreenSize.y) * 2.0f; // converts y to [-1, 1] (flip axis)
	// shader uniforms
	m_BodyShader.SetVec2("Offset", Pos);
	m_BodyShader.SetFloat("ScreenRatio", Camera.m_ScreenSize.x / Camera.m_ScreenSize.y);
	m_BodyShader.SetVec3("LightDir", pLightBody == pBody ? glm::vec3(0, 0, 0) : glm::normalize((glm::vec3)((pLightBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_Radius)));
	m_BodyShader.SetVec3("CameraPos", glm::normalize(Camera.m_Position));

	float Time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - StartTime).count() / 1e9; // Time in seconds
	m_BodyShader.SetFloat("Time", Time);

	glBindVertexArray(m_BodyShader.VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CStarSystem::RenderBodies(CCamera &Camera)
{
	m_BodyShader.Use();
	glDisable(GL_DEPTH_TEST);
	for(auto &Body : m_vBodies)
		RenderBody(&Body, &m_vBodies.front(), Camera);
	glEnable(GL_DEPTH_TEST);
}

void CStarSystem::InitGfx()
{
	m_BodyShader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	float aVertices[] = {
		-1.0f, -1.0f, // Bottom-left
		1.0f, -1.0f, // Bottom-right
		1.0f, 1.0f, // Top-right
		-1.0f, 1.0f // Top-left
	};

	unsigned int aIndices[] = {
		0, 1, 2, // First triangle
		2, 3, 0 // Second triangle
	};

	glGenVertexArrays(1, &m_BodyShader.VAO);
	glGenBuffers(1, &m_BodyShader.VBO);
	glGenBuffers(1, &m_BodyShader.EBO);

	glBindVertexArray(m_BodyShader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_BodyShader.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(aVertices), aVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BodyShader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(aIndices), aIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

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
			}),
		SBody(id++, "Mercury",
			{
				3.3011e23,
				Vec3(5.791e10, 0, 0),
				Vec3(0, 0, 47870),
				Vec3(0, 0, 0)},
			{
				2.4397e6,
			}),
		SBody(id++, "Venus",
			{
				4.8675e24,
				Vec3(1.082e11, 0, 0),
				Vec3(0, 0, 35020),
				Vec3(0, 0, 0)},
			{
				6.0518e6,
			}),
		SBody(id++, "Earth",
			{
				5.972e24,
				Vec3(1.496e11, 0, 0),
				Vec3(0, 0, 29780),
				Vec3(0, 0, 0)},
			{
				6.371e6,
			}),
		SBody(id++, "Moon",
			{
				7.342e22,
				Vec3(1.496e11 + 3.844e8, 0, 0),
				Vec3(0, 0, 29780 + 1022),
				Vec3(0, 0, 0)},
			{
				1.737e6,
			}),
		SBody(id++, "Jupiter",
			{
				1.8982e27,
				Vec3(7.785e11, 0, 0),
				Vec3(0, 0, 13070),
				Vec3(0, 0, 0)},
			{
				6.9911e7,
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
