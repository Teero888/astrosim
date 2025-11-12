#include "starsystem.h"
#include "../gfx/camera.h"
#include "../gfx/graphics.h"
#include "../gfx/shader.h"
#include "body.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/vector_float3.hpp"
#include <chrono>
#include <cstdio>

constexpr double G = 6.67430e-11; // gravitational constant

#include "vmath.h"

static const std::chrono::time_point<std::chrono::steady_clock> StartTime = std::chrono::steady_clock::now();
void CStarSystem::RenderBody(SBody *pBody, SBody *pLightBody, CCamera &Camera)
{
	// set transformation matrices
	glm::mat4 Model = glm::mat4(1.0f);
	Vec3 NewPos = (pBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_Radius;
	Model = glm::translate(Model, (glm::vec3)NewPos);

	m_BodyShader.SetBool("uSource", pBody == pLightBody);
	m_BodyShader.SetFloat("uRadius", pBody->m_RenderParams.m_Radius / Camera.m_Radius);
	m_BodyShader.SetMat4("uModel", Model);
	m_BodyShader.SetMat4("uView", Camera.m_View);
	m_BodyShader.SetMat4("uProjection", Camera.m_Projection);

	// set light properties
	Vec3 NewLightDir = (pLightBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position).normalize();
	m_BodyShader.SetVec3("uLightDir", (glm::vec3)NewLightDir);
	m_BodyShader.SetVec3("uLightColor", pLightBody->m_RenderParams.m_Color);
	m_BodyShader.SetVec3("uObjectColor", pBody->m_RenderParams.m_Color);

	// bind the vao and draw the sphere
	glBindVertexArray(m_BodyShader.VAO);
	glDrawElements(GL_TRIANGLES, m_BodyShader.m_NumIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CStarSystem::RenderBodies(CCamera &Camera)
{
	m_BodyShader.Use();
	for(auto &Body : m_vBodies)
		RenderBody(&Body, &m_vBodies.front(), Camera);
}

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

static void GenerateSphere(float Radius, int Stacks, int Slices, std::vector<Vertex> &vVertices, std::vector<unsigned int> &vIndices)
{
	vVertices.clear();
	vIndices.clear();

	for(int i = 0; i <= Stacks; ++i)
	{
		float phi = static_cast<float>(i) / Stacks * glm::pi<float>();
		for(int j = 0; j <= Slices; ++j)
		{
			float theta = static_cast<float>(j) / Slices * 2.0f * glm::pi<float>();

			Vertex v;
			v.normal.x = std::sin(phi) * std::cos(theta);
			v.normal.y = std::cos(phi);
			v.normal.z = std::sin(phi) * std::sin(theta);

			v.position = v.normal * Radius;

			vVertices.push_back(v);
		}
	}

	for(int i = 0; i < Stacks; ++i)
	{
		for(int j = 0; j < Slices; ++j)
		{
			int first = (i * (Slices + 1)) + j;
			int second = first + Slices + 1;

			vIndices.push_back(first);
			vIndices.push_back(second);
			vIndices.push_back(first + 1);

			vIndices.push_back(second);
			vIndices.push_back(second + 1);
			vIndices.push_back(first + 1);
		}
	}
}

void CStarSystem::InitGfx()
{
	m_BodyShader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	std::vector<Vertex> vVertices;
	std::vector<unsigned int> vIndices;
	GenerateSphere(1.0f, 32, 32, vVertices, vIndices);
	m_BodyShader.m_NumIndices = vIndices.size();

	// Set up VBO, VAO, and EBO
	glGenVertexArrays(1, &m_BodyShader.VAO);
	glGenBuffers(1, &m_BodyShader.VBO);
	glGenBuffers(1, &m_BodyShader.EBO);

	glBindVertexArray(m_BodyShader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_BodyShader.VBO);
	glBufferData(GL_ARRAY_BUFFER, vVertices.size() * sizeof(Vertex), vVertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BodyShader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, vIndices.size() * sizeof(unsigned int), vIndices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);
}

// clang-format off
void CStarSystem::OnInit()
{
	int id = 0;
	m_vBodies = {
		// Sun
		SBody(id++, "Sun",
			{
				1.989e30, // Mass (kg)
				Vec3(0, 0, 0), // Position (m)
				Vec3(0, 0, 0), // Velocity (m/s)
				Vec3(0, 0, 0) // Acceleration
			},
			{
				6.957e8, // Radius (m)
				glm::vec3(1.0f, 0.95f, 0.85f), // Sun: Yellowish-white
			}),

		// Mercury
		SBody(id++, "Mercury",
			{
				3.3011e23,
				Vec3(5.791e10, 0, 0),
				Vec3(0, 0, 47870),
				Vec3(0, 0, 0)},
			{
				2.4397e6,
				glm::vec3(0.6f, 0.5f, 0.4f), // Mercury: Grayish-brown
			}),

		// Venus
		SBody(id++, "Venus",
			{
				4.8675e24,
				Vec3(1.082e11, 0, 0),
				Vec3(0, 0, 35020),
				Vec3(0, 0, 0)},
			{
				6.0518e6,
				glm::vec3(0.9f, 0.7f, 0.5f), // Venus: Pale yellowish-brown
			}),

		// Earth
		SBody(id++, "Earth",
			{
				5.972e24,
				Vec3(1.496e11, 0, 0),
				Vec3(0, 0, 29780),
				Vec3(0, 0, 0)},
			{
				6.371e6,
				glm::vec3(0.0f, 0.3f, 0.7f), // Earth: Blue
			}),

		// Moon (Earth's Moon)
		SBody(id++, "Moon",
			{
				7.342e22,
				Vec3(1.496e11 + 3.844e8, 0, 0),
				Vec3(0, 0, 29780 + 1022),
				Vec3(0, 0, 0)},
			{
				1.737e6,
				glm::vec3(0.5f, 0.5f, 0.5f), // Moon: Gray
			}),

		// Mars
		SBody(id++, "Mars",
			{
				6.4171e23,
				Vec3(2.279e11, 0, 0),
				Vec3(0, 0, 24070),
				Vec3(0, 0, 0)},
			{
				3.3895e6,
				glm::vec3(0.8f, 0.4f, 0.2f), // Mars: Reddish-brown
			}),

		// Phobos (Moon of Mars)
		SBody(id++, "Phobos",
			{
				1.0659e16,
				Vec3(2.279e11 + 9.377e6, 0, 0),
				Vec3(0, 0, 24070 + 2138),
				Vec3(0, 0, 0)},
			{
				1.123e4,
				glm::vec3(0.5f, 0.4f, 0.3f), // Phobos: Dark gray
			}),

		// Deimos (Moon of Mars)
		SBody(id++, "Deimos",
			{
				1.4762e15,
				Vec3(2.279e11 + 2.345e7, 0, 0),
				Vec3(0, 0, 24070 + 1351),
				Vec3(0, 0, 0)},
			{
				6.2e3,
				glm::vec3(0.5f, 0.4f, 0.3f), // Deimos: Dark gray
			}),

		// Jupiter
		SBody(id++, "Jupiter",
			{
				1.8982e27,
				Vec3(7.785e11, 0, 0),
				Vec3(0, 0, 13070),
				Vec3(0, 0, 0)},
			{
				6.9911e7,
				glm::vec3(0.8f, 0.6f, 0.4f), // Jupiter: Orange-brown
			}),

		// Io (Moon of Jupiter)
		SBody(id++, "Io",
			{
				8.9319e22,
				Vec3(7.785e11 + 4.217e8, 0, 0),
				Vec3(0, 0, 13070 + 17340),
				Vec3(0, 0, 0)},
			{
				1.8216e6,
				glm::vec3(0.9f, 0.7f, 0.5f), // Io: Yellowish (volcanic)
			}),

		// Europa (Moon of Jupiter)
		SBody(id++, "Europa",
			{
				4.7998e22,
				Vec3(7.785e11 + 6.711e8, 0, 0),
				Vec3(0, 0, 13070 + 13740),
				Vec3(0, 0, 0)},
			{
				1.5608e6,
				glm::vec3(0.8f, 0.8f, 0.9f), // Europa: Icy white
			}),

		// Ganymede (Moon of Jupiter)
		SBody(id++, "Ganymede",
			{
				1.4819e23,
				Vec3(7.785e11 + 1.0704e9, 0, 0),
				Vec3(0, 0, 13070 + 10880),
				Vec3(0, 0, 0)},
			{
				2.6341e6,
				glm::vec3(0.6f, 0.6f, 0.7f), // Ganymede: Grayish
			}),

		// Callisto (Moon of Jupiter)
		SBody(id++, "Callisto",
			{
				1.0759e23,
				Vec3(7.785e11 + 1.8827e9, 0, 0),
				Vec3(0, 0, 13070 + 8200),
				Vec3(0, 0, 0)},
			{
				2.4103e6,
				glm::vec3(0.5f, 0.5f, 0.5f), // Callisto: Gray
			}),

		// Saturn
		SBody(id++, "Saturn",
			{
				5.6834e26,
				Vec3(1.429e12, 0, 0),
				Vec3(0, 0, 9680),
				Vec3(0, 0, 0)},
			{
				5.8232e7,
				glm::vec3(0.9f, 0.8f, 0.6f), // Saturn: Pale yellow
			}),

		// Titan (Moon of Saturn)
		SBody(id++, "Titan",
			{
				1.3452e23,
				Vec3(1.429e12 + 1.2218e9, 0, 0),
				Vec3(0, 0, 9680 + 5570),
				Vec3(0, 0, 0)},
			{
				2.5747e6,
				glm::vec3(0.7f, 0.6f, 0.5f), // Titan: Orange-brown
			}),

		// Uranus
		SBody(id++, "Uranus",
			{
				8.6810e25,
				Vec3(2.871e12, 0, 0),
				Vec3(0, 0, 6800),
				Vec3(0, 0, 0)},
			{
				2.5362e7,
				glm::vec3(0.6f, 0.8f, 0.9f), // Uranus: Pale blue
			}),

		// Neptune
		SBody(id++, "Neptune",
			{
				1.0241e26,
				Vec3(4.498e12, 0, 0),
				Vec3(0, 0, 5430),
				Vec3(0, 0, 0)},
			{
				2.4622e7,
				glm::vec3(0.3f, 0.5f, 0.8f), // Neptune: Deep blue
			}),

		// Triton (Moon of Neptune)
		SBody(id++, "Triton",
			{
				2.1390e22,
				Vec3(4.498e12 + 3.5476e8, 0, 0),
				Vec3(0, 0, 5430 + 4400),
				Vec3(0, 0, 0)},
			{
				1.3534e6,
				glm::vec3(0.7f, 0.7f, 0.8f), // Triton: Icy white
			})
	};
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
