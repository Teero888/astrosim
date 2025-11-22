#include "grid.h"
#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include <cmath>
#include <embedded_shaders.h>

#define GRID_RESOLUTION 1

void CGrid::Init()
{
	m_Shader.CompileShader(Shaders::VERT_GRID, Shaders::FRAG_GRID);

	const float vertices[] = {
		// Positions
		-1.0f, 1.0f,
		-1.0f, -1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f};

	const unsigned int indices[] = {
		0, 1, 2,
		0, 2, 3};

	// Create and bind buffers
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

void CGrid::Render(CStarSystem &System, CCamera &Camera)
{
	m_Shader.Use();

	// Pass constant needed for logarithmic depth calculation
	float F = 2.0f / log2(FAR_PLANE + 1.0f); // far plane is 10000.0f
	m_Shader.SetFloat("u_logDepthF", F);

	// A view matrix without translation or zoom scaling, for calculating the ray direction.
	glm::mat4 gridView = glm::lookAt(glm::vec3(0.0f), (glm::vec3)Camera.m_Front, (glm::vec3)Camera.m_Up);

	m_Shader.SetMat4("u_invView", glm::inverse(gridView));
	m_Shader.SetMat4("u_invProjection", glm::inverse(Camera.m_Projection));

	// Pass the main camera matrices for gl_FragDepth calculation
	m_Shader.SetMat4("u_renderView", Camera.m_View);
	m_Shader.SetMat4("u_renderProjection", Camera.m_Projection);

	// Pass camera position as a high-precision double uniform.
	GLint loc = glGetUniformLocation(m_Shader.m_Program, "u_cameraPos");
	glUniform3dv(loc, 1, &Camera.m_AbsolutePosition.x);

	m_Shader.SetFloat("u_viewDistance", (float)Camera.m_ViewDistance);

	// Pass focused body info for planet cutout.
	SBody *pFocusedBody = Camera.m_pFocusedBody;

	GLint body_loc = glGetUniformLocation(m_Shader.m_Program, "u_focusedBodyPos");
	glUniform3dv(body_loc, 1, &pFocusedBody->m_SimParams.m_Position.x);
	m_Shader.SetFloat("u_focusedBodyRadius", (float)pFocusedBody->m_RenderParams.m_Radius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE); // Don't write to depth buffer

	glBindVertexArray(m_Shader.VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE); // Restore depth writing
	glDisable(GL_BLEND);
}

void CGrid::Destroy()
{
	m_Shader.Destroy();
}
