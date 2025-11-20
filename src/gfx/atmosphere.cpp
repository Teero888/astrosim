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
	m_BakerShader.CompileShader(Shaders::VERT_FULLSCREEN, Shaders::FRAG_TRANSMITTANCE);

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

	// 4. Init LUT Texture and FBO
	glGenFramebuffers(1, &m_LUT_FBO);
	glGenTextures(1, &m_LUT_Texture);

	glBindTexture(GL_TEXTURE_2D, m_LUT_Texture);
	// High precision RG32F: R=Rayleigh, G=Mie
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, 512, 512, 0, GL_RG, GL_FLOAT, NULL);

	// CLAMP_TO_EDGE is critical to avoid artifacts at zenith/nadir
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_FRAMEBUFFER, m_LUT_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_LUT_Texture, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("ERROR::ATMOSPHERE::LUT FBO Incomplete\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CAtmosphere::BakeLUT(const SBody &Body)
{
	// Save previous viewport
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glBindFramebuffer(GL_FRAMEBUFFER, m_LUT_FBO);
	glViewport(0, 0, 512, 512);
	glClear(GL_COLOR_BUFFER_BIT);

	m_BakerShader.Use();

	// Pass Physics params relative to Unit Space (Radius = 1.0)
	double realRadius = Body.m_RenderParams.m_Radius;
	m_BakerShader.SetFloat("u_atmosphereRadius", Body.m_RenderParams.m_Atmosphere.m_AtmosphereRadius);
	m_BakerShader.SetFloat("u_rayleighScaleHeight", Body.m_RenderParams.m_Atmosphere.m_RayleighScaleHeight / realRadius);
	m_BakerShader.SetFloat("u_mieScaleHeight", Body.m_RenderParams.m_Atmosphere.m_MieScaleHeight / realRadius);

	glBindVertexArray(m_Shader.VAO); // Re-use the quad
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Restore viewport
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

	m_LastBakedBodyId = Body.m_Id;
	// printf("Baked Transmittance LUT for %s\n", Body.m_Name.c_str());
}

void CAtmosphere::Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit)
{
	SBody *pFocusedBody = Camera.m_pFocusedBody;

	if(!pFocusedBody || !pFocusedBody->m_RenderParams.m_Atmosphere.m_Enabled)
		return;

	// Check if we need to re-bake (Optimized to only happen on switch)
	// Note: If you change atmosphere params in ImGui, you might need to force this logic or add a "Dirty" flag
	if(m_LastBakedBodyId != pFocusedBody->m_Id)
		BakeLUT(*pFocusedBody);
	m_Shader.Use();

	// Bind LUT to Texture Unit 1 (Depth is Unit 0)
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_LUT_Texture);
	m_Shader.SetInt("u_transmittanceLUT", 1);

	// Pass Depth Texture
	glActiveTexture(GL_TEXTURE0 + DepthTextureUnit); // Should be 0 from calling code, but safe here
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
	m_Shader.SetFloat("u_sunIntensity", pFocusedBody->m_RenderParams.m_Atmosphere.m_SunIntensity);
	m_Shader.SetFloat("u_atmosphereRadius", pFocusedBody->m_RenderParams.m_Atmosphere.m_AtmosphereRadius);

	m_Shader.SetVec3("u_rayleighScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.m_RayleighScatteringCoeff);
	m_Shader.SetFloat("u_rayleighScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.m_RayleighScaleHeight / realRadius);
	m_Shader.SetVec3("u_mieScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.m_MieScatteringCoeff);
	m_Shader.SetFloat("u_mieScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.m_MieScaleHeight / realRadius);
	m_Shader.SetFloat("u_miePreferredScatteringDir", pFocusedBody->m_RenderParams.m_Atmosphere.m_MiePreferredScatteringDir);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

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
	if(m_LUT_FBO)
		glDeleteFramebuffers(1, &m_LUT_FBO);
	if(m_LUT_Texture)
		glDeleteTextures(1, &m_LUT_Texture);
	m_BakerShader.Destroy();
	m_Shader.Destroy();
}
