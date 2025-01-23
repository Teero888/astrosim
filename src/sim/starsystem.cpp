#include "starsystem.h"
#include "../gfx/camera.h"
#include "../gfx/shader.h"
#include "body.h"
#include "glm/ext/vector_float3.hpp"
#include <cstdio>

constexpr double G = 6.67430e-11; // gravitational constant
// basically the speed/accuracy of the simultion
constexpr double dt = 60.0 * 60.0; // Time step (1hr = 1tick)

#include "vmath.h"

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_Position - a.m_Position; // Vector from a to b
	const double Distance = r.length();
	const double ForceMagnitude = (G * a.m_Mass * b.m_Mass) / (Distance * Distance); // F = G * m1 * m2 / r^2
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

void CStarSystem::RenderBody(SBody *pBody, SBody *pLightBody, CShader *pShader, class CCamera *pCamera)
{
	pShader->Use();

	// set transformation matrices
	glm::mat4 Model = glm::mat4(1.0f);
	Vec3 NewPos = (pBody->m_Position - pCamera->m_pFocusedBody->m_Position) / RENDER_SCALE;
	Model = glm::translate(Model, (glm::vec3)NewPos);

	pShader->SetBool("Source", pBody == pLightBody);
	pShader->SetFloat("Radius", pBody->m_Radius / RENDER_SCALE);
	pShader->SetMat4("model", Model);
	pShader->SetMat4("view", pCamera->m_View);
	pShader->SetMat4("projection", pCamera->m_Projection);

	// set light properties
	Vec3 NewLightPos = pLightBody->m_Position - pCamera->m_pFocusedBody->m_Position;
	pShader->SetVec3("lightPos", (glm::vec3)(NewLightPos / RENDER_SCALE));
	pShader->SetVec3("lightColor", pLightBody->m_Color);
	pShader->SetVec3("objectColor", pBody->m_Color);

	// bind the vao and draw the sphere
	glBindVertexArray(pShader->VAO);
	glDrawElements(GL_TRIANGLES, pShader->m_NumIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CStarSystem::OnInit()
{
	int Id = 0;
	m_vBodies = {
		// Sun
		SBody(Id++, "Sun", 1.989e30, 6.957e8, Vec3(0, 0, 0), Vec3(0, 0, 0), glm::vec3(1.0, 0.8, 0.1)),

		// Mercury (no moons)
		SBody(Id++, "Mercury", 3.3011e23, 2.4397e6, Vec3(5.791e10, 0, 0), Vec3(0, 0, 47870), glm::vec3(0.7, 0.5, 0.5)),

		// Venus (no moons)
		SBody(Id++, "Venus", 4.8675e24, 6.0518e6, Vec3(1.082e11, 0, 0), Vec3(0, 0, 35020), glm::vec3(0.9, 0.7, 0.3)),

		// Earth + Moon
		SBody(Id++, "Earth", 5.972e24, 6.371e6, Vec3(1.496e11, 0, 0), Vec3(0, 0, 29780), glm::vec3(0.1, 0.1, 1.0)),
		SBody(Id++, "Moon", 7.342e22, 1.737e6, Vec3(1.496e11 + 3.844e8, 0, 0), Vec3(0, 0, 29780 + 1022), glm::vec3(0.5, 0.5, 0.5)),

		// Mars + Moons
		SBody(Id++, "Mars", 6.4171e23, 3.3895e6, Vec3(2.279e11, 0, 0), Vec3(0, 0, 24070), glm::vec3(0.8, 0.3, 0.1)),
		SBody(Id++, "Phobos", 1.0659e16, 11.267e3, Vec3(2.279e11 + 9.377e6, 0, 0), Vec3(0, 0, 24070 + 2138), glm::vec3(0.6, 0.6, 0.6)),
		SBody(Id++, "Deimos", 1.4762e15, 6.2e3, Vec3(2.279e11 + 2.346e7, 0, 0), Vec3(0, 0, 24070 + 1351), glm::vec3(0.7, 0.7, 0.7)),

		// Jupiter + Galilean Moons
		SBody(Id++, "Jupiter", 1.8982e27, 6.9911e7, Vec3(7.785e11, 0, 0), Vec3(0, 0, 13070), glm::vec3(0.8, 0.6, 0.4)),
		SBody(Id++, "Io", 8.9319e22, 1.8216e6, Vec3(7.785e11 + 4.217e8, 0, 0), Vec3(0, 0, 13070 + 17334), glm::vec3(0.9, 0.8, 0.2)),
		SBody(Id++, "Europa", 4.7998e22, 1.561e6, Vec3(7.785e11 + 6.709e8, 0, 0), Vec3(0, 0, 13070 + 13740), glm::vec3(0.8, 0.8, 0.9)),
		SBody(Id++, "Ganymede", 1.4819e23, 2.631e6, Vec3(7.785e11 + 1.0704e9, 0, 0), Vec3(0, 0, 13070 + 10880), glm::vec3(0.6, 0.6, 0.7)),
		SBody(Id++, "Callisto", 1.0759e23, 2.4103e6, Vec3(7.785e11 + 1.8827e9, 0, 0), Vec3(0, 0, 13070 + 8204), glm::vec3(0.5, 0.5, 0.6)),

		// Saturn + Moons
		SBody(Id++, "Saturn", 5.6834e26, 5.8232e7, Vec3(1.429e12, 0, 0), Vec3(0, 0, 9690), glm::vec3(0.9, 0.8, 0.5)),
		SBody(Id++, "Titan", 1.3452e23, 2.5747e6, Vec3(1.429e12 + 1.221e9, 0, 0), Vec3(0, 0, 9690 + 5570), glm::vec3(0.8, 0.7, 0.3)),
		SBody(Id++, "Enceladus", 1.0802e20, 2.521e5, Vec3(1.429e12 + 2.38e8, 0, 0), Vec3(0, 0, 9690 + 12600), glm::vec3(0.9, 0.9, 0.9)),

		// Uranus + Moons
		SBody(Id++, "Uranus", 8.6810e25, 2.5362e7, Vec3(2.871e12, 0, 0), Vec3(0, 0, 6810), glm::vec3(0.4, 0.6, 0.8)),
		SBody(Id++, "Titania", 3.4e21, 7.887e5, Vec3(2.871e12 + 4.356e8, 0, 0), Vec3(0, 0, 6810 + 3640), glm::vec3(0.7, 0.7, 0.8)),
		SBody(Id++, "Oberon", 3.076e21, 7.618e5, Vec3(2.871e12 + 5.835e8, 0, 0), Vec3(0, 0, 6810 + 3010), glm::vec3(0.6, 0.6, 0.7)),

		// Neptune + Triton
		SBody(Id++, "Neptune", 1.02413e26, 2.4622e7, Vec3(4.498e12, 0, 0), Vec3(0, 0, 5430), glm::vec3(0.2, 0.3, 0.9)),
		SBody(Id++, "Triton", 2.14e22, 1.3534e6, Vec3(4.498e12 + 3.547e8, 0, 0), Vec3(0, 0, 5430 + 4390), glm::vec3(0.7, 0.7, 0.8))};
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
		RenderBody(&Body, &m_vBodies.front(), pShader, pCamera);
}
