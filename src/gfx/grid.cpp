#include "grid.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include <cmath>

#define GRID_RESOLUTION 1

void CGrid::Init()
{
	m_Shader.CompileShader(Shaders::VERT_GRID, Shaders::FRAG_GRID);

	const float STEP = 2.0f / GRID_RESOLUTION;

	std::vector<float> vertices;
	for(int i = 0; i <= GRID_RESOLUTION; ++i)
	{
		for(int j = 0; j <= GRID_RESOLUTION; ++j)
		{
			vertices.push_back(-1.0f + j * STEP); // X
			vertices.push_back(0.0f); // Y
			vertices.push_back(-1.0f + i * STEP); // Z
		}
	}

	std::vector<unsigned int> indices;
	for(int i = 0; i < GRID_RESOLUTION; ++i)
	{
		for(int j = 0; j < GRID_RESOLUTION; ++j)
		{
			int topLeft = i * (GRID_RESOLUTION + 1) + j;
			int topRight = topLeft + 1;
			int bottomLeft = (i + 1) * (GRID_RESOLUTION + 1) + j;
			int bottomRight = bottomLeft + 1;

			// First triangle
			indices.push_back(topLeft);
			indices.push_back(bottomLeft);
			indices.push_back(topRight);

			// Second triangle
			indices.push_back(topRight);
			indices.push_back(bottomLeft);
			indices.push_back(bottomRight);
		}
	}

	// Create and bind buffers
	glGenVertexArrays(1, &m_Shader.VAO);
	glGenBuffers(1, &m_Shader.VBO);
	glGenBuffers(1, &m_Shader.EBO);

	glBindVertexArray(m_Shader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_Shader.VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Shader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void CGrid::Render(CStarSystem &System, CCamera &Camera)
{
	m_Shader.Use();

	constexpr float ZOOM_STEP = 10.0f; // every 10 zooms grid updates
	static const float STEP_FACTOR = std::pow(1.f + (0.1f), ZOOM_STEP);
	static const float LOG_STEP = std::log(STEP_FACTOR);

	const float BodyRadius = Camera.m_pFocusedBody->m_RenderParams.m_Radius;
	const float InitialScale = Camera.m_ViewDistance / BodyRadius;
	int QuantizedScale = std::pow(STEP_FACTOR, (int)(std::log(InitialScale) / LOG_STEP));

	const float GridScale = (BodyRadius / Camera.m_ViewDistance) * std::max(QuantizedScale * 10.f, 1.f);
	m_Shader.SetFloat("GridScale", GridScale);

	const Vec3 GridPos = Camera.m_pFocusedBody->m_SimParams.m_Position / (GridScale * Camera.m_ViewDistance);
	const Vec3 Offset = (GridPos - GridPos.floor()) * GridScale;

	glm::mat4 Model = glm::mat4(1.0f);
	Model = glm::translate(Model, (glm::vec3)Offset);
	m_Shader.SetMat4("Model", Model);

	m_Shader.SetFloat("CameraScale", BodyRadius / Camera.m_ViewDistance);
	m_Shader.SetFloat("Scale", DEFAULT_SCALE * 2);
	m_Shader.SetVec3("GridColor", glm::vec3(0.4f));
	m_Shader.SetMat4("View", Camera.m_View);
	m_Shader.SetMat4("Projection", Camera.m_Projection);

	glBindVertexArray(m_Shader.VAO);
	glDrawElements(GL_TRIANGLES, GRID_RESOLUTION * GRID_RESOLUTION * 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CGrid::Destroy()
{
	m_Shader.Destroy();
}
