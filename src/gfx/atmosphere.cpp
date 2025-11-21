#include "atmosphere.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include <cmath>
#include <embedded_shaders.h>

// [FIX] Include GLM quaternion headers for matrix conversion
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

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

void CAtmosphere::Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit, unsigned int ShadowTextureID, const glm::mat4 &LightSpaceMatrix, int DebugMode)
{
	SBody *pFocusedBody = Camera.m_pFocusedBody;

	if(!pFocusedBody || !pFocusedBody->m_RenderParams.m_Atmosphere.m_Enabled)
		return;

	m_Shader.Use();

	// We just tell the shader which unit it is on.
	m_Shader.SetInt("u_depthTexture", DepthTextureUnit);

	m_Shader.SetVec2("u_screenSize", Camera.m_ScreenSize);
	m_Shader.SetInt("u_debugMode", DebugMode);

	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	// [FIX] Calculate View-to-Local Space Matrix
	// 1. Get Planet Rotation
	Quat q = pFocusedBody->m_SimParams.m_Orientation;
	// 2. Get Inverse Rotation (World -> Local)
	Quat qInv = q.Conjugate();
	glm::quat glmQInv(qInv.w, qInv.x, qInv.y, qInv.z);
	glm::mat4 matRotInv = glm::mat4_cast(glmQInv);

	// 3. Standard View -> World Matrix (Rotation Only)
	glm::mat4 gridView = glm::lookAt(glm::vec3(0.0f), (glm::vec3)Camera.m_Front, (glm::vec3)Camera.m_Up);
	glm::mat4 viewToWorld = glm::inverse(gridView);

	// 4. Combine: View -> World -> Local
	// This ensures v_rayDirection in the shader is in Planet Local Space
	glm::mat4 viewToLocal = matRotInv * viewToWorld;

	m_Shader.SetMat4("u_invView", viewToLocal);
	m_Shader.SetMat4("u_invProjection", glm::inverse(Camera.m_Projection));

	double realRadius = pFocusedBody->m_RenderParams.m_Radius;
	// Calculate Camera Position relative to planet center in Unit Space (Scale 1.0)
	Vec3 relativeCamPos = Camera.m_AbsolutePosition - pFocusedBody->m_SimParams.m_Position;

	// [FIX] Rotate Camera Position into Planet Local Space
	Vec3 localCamPos = qInv.RotateVector(relativeCamPos);
	Vec3 normalizedCamPos = localCamPos / realRadius;

	m_Shader.SetVec3("u_cameraPos", (glm::vec3)normalizedCamPos);

	// [FIX] Safety check for sun direction to avoid NaN if focusing on Sun
	Vec3 sunVector = System.m_pSunBody->m_SimParams.m_Position - pFocusedBody->m_SimParams.m_Position;
	Vec3 sunDir;
	if(sunVector.length() > 1.0)
	{
		// [FIX] Rotate Sun Direction into Planet Local Space
		sunDir = qInv.RotateVector(sunVector.normalize());
	}
	else
	{
		sunDir = Vec3(0, 1, 0); // Fallback
	}
	m_Shader.SetVec3("u_sunDirection", (glm::vec3)sunDir);

	// Pass real radius in meters for physics calcs
	m_Shader.SetFloat("u_realPlanetRadius", (float)realRadius);

	// Physics params (Meters)
	m_Shader.SetFloat("u_sunIntensity", pFocusedBody->m_RenderParams.m_Atmosphere.m_SunIntensity);

	float atmRadMeters = pFocusedBody->m_RenderParams.m_Atmosphere.m_AtmosphereRadius * (float)realRadius;

	m_Shader.SetFloat("u_atmosphereRadius", atmRadMeters);

	m_Shader.SetVec3("u_rayleighScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.m_RayleighScatteringCoeff);
	m_Shader.SetFloat("u_rayleighScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.m_RayleighScaleHeight);
	m_Shader.SetVec3("u_mieScatteringCoeff", pFocusedBody->m_RenderParams.m_Atmosphere.m_MieScatteringCoeff);
	m_Shader.SetFloat("u_mieScaleHeight", pFocusedBody->m_RenderParams.m_Atmosphere.m_MieScaleHeight);
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
	m_Shader.Destroy();
}
