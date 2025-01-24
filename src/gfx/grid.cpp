#include "grid.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include <cmath>
#include <cstdio>

void CGrid::Init()
{
	m_Shader.CompileShader(Shaders::VERT_GRID, Shaders::FRAG_GRID);

	float Scale = 1;
	float vertices[] = {
		-1.0f * Scale, 0.0f, -1.0f * Scale, // Bottom-left
		1.0f * Scale, 0.0f, -1.0f * Scale, // Bottom-right
		1.0f * Scale, 0.0f, 1.0f * Scale, // Top-right
		-1.0f * Scale, 0.0f, 1.0f * Scale // Top-left
	};

	unsigned int indices[] = {
		0, 1, 2, // First triangle
		2, 3, 0 // Second triangle
	};

	// Generate and bind the VAO, VBO, and EBO
	glGenVertexArrays(1, &m_Shader.VAO);
	glGenBuffers(1, &m_Shader.VBO);
	glGenBuffers(1, &m_Shader.EBO);

	glBindVertexArray(m_Shader.VAO);

	// Bind and set the vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, m_Shader.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Bind and set the element buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Shader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Set the vertex attribute pointers
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void CGrid::Render(CCamera &Camera)
{
	m_Shader.Use();

	// using static const instead of constexpr since math funcs no work
	constexpr float ZOOM_STEP = 10.0f; // every 10 zooms grid updates
	static const float STEP_FACTOR = std::pow(1.f + (1.f / ZOOM_FACTOR), ZOOM_STEP);
	static const float LOG_STEP = std::log(STEP_FACTOR);

	const float BodyRadius = Camera.m_pFocusedBody->m_Radius;
	const float InitialScale = Camera.m_Radius / BodyRadius;
	int QuantizedScale = std::pow(STEP_FACTOR, (int)(std::log(InitialScale) / LOG_STEP));

	const float GridScale = (BodyRadius / Camera.m_Radius) * std::max(QuantizedScale * 10.f, 1.f);
	m_Shader.SetFloat("GridScale", GridScale);

	const Vec3 GridPos = Camera.m_pFocusedBody->m_Position / (GridScale * Camera.m_Radius);
	const Vec3 Offset = (GridPos % 1) * GridScale;

	glm::mat4 Model = glm::mat4(1.0f);
	Model = glm::translate(Model, (glm::vec3)Offset);
	m_Shader.SetMat4("Model", Model);

	m_Shader.SetFloat("Scale", DEFAULT_SCALE);
	m_Shader.SetVec3("GridColor", glm::vec3(0.4f));
	m_Shader.SetMat4("View", Camera.m_View);
	m_Shader.SetMat4("Projection", Camera.m_Projection);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_Shader.VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CGrid::Destroy()
{
	m_Shader.Destroy();
}
