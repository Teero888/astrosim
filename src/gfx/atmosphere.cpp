#include "atmosphere.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include <cmath>
#include <embedded_shaders.h>

void CAtmosphere::Init()
{
	m_Shader.CompileShader(Shaders::VERT_ATMOSPHERE, Shaders::FRAG_ATMOSPHERE);
	const float vertices[] = {-1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
	const unsigned int indices[] = {0, 1, 2, 0, 2, 3};

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

void CAtmosphere::Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit)
{
	SBody *pFocusedBody = Camera.m_pFocusedBody;

	// Check new Enabled flag
	if(!pFocusedBody || !pFocusedBody->m_RenderParams.m_Atmosphere.Enabled)
		return;

	m_Shader.Use();

	// Pass Depth Texture
	m_Shader.SetInt("u_depthTexture", DepthTextureUnit);
	m_Shader.SetVec2("u_screenSize", Camera.m_ScreenSize);

	// Logarithmic Depth Constant
	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	glm::mat4 gridView = glm::lookAt(glm::vec3(0.0f), (glm::vec3)Camera.m_Front, (glm::vec3)Camera.m_Up);
	m_Shader.SetMat4("u_invView", glm::inverse(gridView));
	m_Shader.SetMat4("u_invProjection", glm::inverse(Camera.m_Projection));

	double realRadius = pFocusedBody->m_RenderParams.m_Radius;
	Vec3 relativeCamPos = Camera.m_AbsolutePosition - pFocusedBody->m_SimParams.m_Position;
	Vec3 normalizedCamPos = relativeCamPos / realRadius;

	m_Shader.SetVec3("u_cameraPos", (glm::vec3)normalizedCamPos);
	Vec3 sunDir = (System.m_pSunBody->m_SimParams.m_Position - pFocusedBody->m_SimParams.m_Position).normalize();
	m_Shader.SetVec3("u_sunDirection", (glm::vec3)sunDir);

	m_Shader.SetFloat("u_realPlanetRadius", (float)realRadius);

	// Pass new customizable intensity and radius
	m_Shader.SetFloat("u_sunIntensity", pFocusedBody->m_RenderParams.m_Atmosphere.SunIntensity);
	m_Shader.SetFloat("u_atmosphereRadius", pFocusedBody->m_RenderParams.m_Atmosphere.AtmosphereRadius);

	m_Shader.SetVec3("u_rayleighScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.RayleighScatteringCoeff);
	m_Shader.SetFloat("u_rayleighScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.RayleighScaleHeight / realRadius);
	m_Shader.SetVec3("u_mieScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.MieScatteringCoeff);
	m_Shader.SetFloat("u_mieScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.MieScaleHeight / realRadius);
	m_Shader.SetFloat("u_miePreferredScatteringDir", pFocusedBody->m_RenderParams.m_Atmosphere.MiePreferredScatteringDir);

	glEnable(GL_BLEND);
	// Physics-based blending: Color + Dest * Transmittance (where SrcAlpha = 1-Transmittance)
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// ENABLE Depth Test, but disable write
	//glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glBindVertexArray(m_Shader.VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void CAtmosphere::Destroy()
{
	m_Shader.Destroy();
}
