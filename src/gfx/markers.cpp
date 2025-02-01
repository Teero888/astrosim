#include "markers.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/fwd.hpp"
#include "graphics.h"
#include <algorithm>
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
	m_Shader.Use();
	glDisable(GL_DEPTH_TEST);
	for(auto &Body : System.m_vBodies)
	{
		const Vec3 Distance = (Body.m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) + Camera.m_Radius;
		const float Radius = (Body.m_RenderParams.m_Radius / Distance.length());
		const float Min = Radius / DEFAULT_SCALE;
		m_Shader.SetVec3("Color", glm::vec3(1.0f));
		m_Shader.SetFloat("Scale", std::max(Min, 0.01f) * 4);

		const Vec3 NewPos = (Body.m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_Radius;
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
