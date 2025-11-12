#include "markers.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "graphics.h"
#include <cstdio>

void CMarkers::Init()
{
	m_Shader.CompileShader(Shaders::VERT_MARKER, Shaders::FRAG_MARKER);

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

	glGenVertexArrays(1, &m_Shader.VAO);
	glGenBuffers(1, &m_Shader.VBO);
	glGenBuffers(1, &m_Shader.EBO);

	glBindVertexArray(m_Shader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_Shader.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(aVertices), aVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Shader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(aIndices), aIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void CMarkers::Render(CStarSystem &System, CCamera &Camera)
{
	if (!m_ShowMarkers)
		return;

	m_Shader.Use();
	glDisable(GL_DEPTH_TEST);
	for(auto &Body : System.m_vBodies)
	{
		// Calculate screen position
		glm::vec3 WorldPos = (glm::vec3)((Body.m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_ViewDistance);
		glm::vec2 ScreenPos = WorldToScreenCoordinates(WorldPos, glm::mat4(1.f), Camera.m_View, Camera.m_Projection, Camera.m_ScreenSize.x, Camera.m_ScreenSize.y);

		// Check if marker is on screen
		if(ScreenPos.x < 0 || ScreenPos.y < 0 ||
			ScreenPos.x > Camera.m_ScreenSize.x || ScreenPos.y > Camera.m_ScreenSize.y)
			continue;
		
		// Calculate screen size
		// Project a point at the edge of the body to screen coordinates
		// Use the camera's right vector to find a point on the edge
		Vec3 WorldEdgeOffset = (Vec3)Camera.m_Right * Body.m_RenderParams.m_Radius;
		glm::vec3 WorldEdgePos = (glm::vec3)(((Body.m_SimParams.m_Position + WorldEdgeOffset) - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_ViewDistance);
		glm::vec2 ScreenEdgePos = WorldToScreenCoordinates(WorldEdgePos, glm::mat4(1.f), Camera.m_View, Camera.m_Projection, Camera.m_ScreenSize.x, Camera.m_ScreenSize.y);
		
		float ScreenRadius = glm::distance(ScreenPos, ScreenEdgePos);
		float MinScale = (5.0f * 2.0f) / Camera.m_ScreenSize.x; // 5 pixels diameter in NDC
		float Scale = glm::max(MinScale, (ScreenRadius * 2.0f) / Camera.m_ScreenSize.x); // Convert to NDC diameter

		glm::vec2 Pos;
		Pos.x = (ScreenPos.x / Camera.m_ScreenSize.x) * 2.0f - 1.0f; // converts x to [-1, 1]
		Pos.y = 1.0f - (ScreenPos.y / Camera.m_ScreenSize.y) * 2.0f; // converts y to [-1, 1] (flip axis)
		m_Shader.SetVec2("Offset", Pos);
		m_Shader.SetFloat("ScreenRatio", Camera.m_ScreenSize.x / Camera.m_ScreenSize.y);
		m_Shader.SetVec3("Color", glm::vec3(1.0f));
		m_Shader.SetFloat("Scale", Scale);

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
