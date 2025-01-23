#include "markers.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/fwd.hpp"
#include <algorithm>
#include <cstdio>

void CMarkers::Init()
{
	m_Shader.CompileShader(Shaders::VERT_MARKER, Shaders::FRAG_MARKER);

	float vertices[] = {
		-1.0f, -1.0f, // Bottom-left
		1.0f, -1.0f, // Bottom-right
		1.0f, 1.0f, // Top-right
		-1.0f, 1.0f // Top-left
	};

	unsigned int indices[] = {
		0, 1, 2, // First triangle
		2, 3, 0 // Second triangle
	};

	glGenVertexArrays(1, &m_Shader.VAO);
	glGenBuffers(1, &m_Shader.VBO);
	glGenBuffers(1, &m_Shader.EBO);

	glBindVertexArray(m_Shader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_Shader.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Shader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

static glm::vec2 WorldToScreenCoordinates(glm::vec3 WorldPos, glm::mat4 Model, glm::mat4 View, glm::mat4 Projection, int ScreenWidth, int ScreenHeight)
{
	glm::mat4 mvp = Projection * View * Model;
	glm::vec4 clipSpacePos = mvp * glm::vec4(WorldPos, 1.0f);
	// outside of camera view
	if(clipSpacePos.w < 0.f)
		return glm::vec2(-1.f);
	glm::vec3 ndcPos = glm::vec3(clipSpacePos) / clipSpacePos.w;
	glm::vec2 screenPos;
	screenPos.x = (ndcPos.x + 1.0f) * 0.5f * ScreenWidth;
	screenPos.y = (1.0f - ndcPos.y) * 0.5f * ScreenHeight;
	return screenPos;
}

void CMarkers::Render(CStarSystem &System, CCamera &Camera)
{
	m_Shader.Use();
	glDisable(GL_DEPTH_TEST);
	for(auto &Body : System.m_vBodies)
	{
		const Vec3 NewPos = (Body.m_Position - Camera.m_pFocusedBody->m_Position) / RENDER_SCALE;
		const float Radius = (Body.m_Radius / RENDER_SCALE);
		const float Min = Body.m_Id == Camera.m_pFocusedBody->m_Id ? Radius / Camera.m_Radius : Radius / NewPos.length();
		m_Shader.SetVec3("Color", glm::vec3(1.0f));
		m_Shader.SetFloat("Scale", std::max(Min, 0.01f) * 2);

		glm::vec2 ScreenPos = WorldToScreenCoordinates(NewPos, glm::mat4(1.f), Camera.m_View, Camera.m_Projection, Camera.m_ScreenSize.x, Camera.m_ScreenSize.y);
		if(ScreenPos.x < 0 || ScreenPos.y < 0 ||
			ScreenPos.x > Camera.m_ScreenSize.x || ScreenPos.y > Camera.m_ScreenSize.y)
			continue;

		glm::vec2 Pos;
		Pos.x = (ScreenPos.x / Camera.m_ScreenSize.x) * 2.0f - 1.0f; // converts x to [-1, 1]
		Pos.y = 1.0f - (ScreenPos.y / Camera.m_ScreenSize.y) * 2.0f; // converts y to [-1, 1] (flip axis)
		m_Shader.SetVec2("Offset", Pos);
		m_Shader.SetFloat("ScreenRatio", Camera.m_ScreenSize.x / Camera.m_ScreenSize.y);

		glBindVertexArray(m_Shader.VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
	glEnable(GL_DEPTH_TEST);
}

void CMarkers::Destroy()
{
	m_Shader.Destroy();
}
