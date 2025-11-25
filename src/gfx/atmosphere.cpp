#include "atmosphere.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include <algorithm>
#include <cmath>
#include <embedded_shaders.h>
#include <vector>

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
	m_Shader.Use();

	m_Shader.SetInt("u_depthTexture", DepthTextureUnit);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ShadowTextureID);
	m_Shader.SetInt("u_shadowMap", 1);

	m_Shader.SetMat4("u_lightSpaceMatrix", LightSpaceMatrix);
	m_Shader.SetVec2("u_screenSize", Camera.m_ScreenSize);
	m_Shader.SetInt("u_debugMode", DebugMode);

	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glBindVertexArray(m_Shader.VAO);

	// Collect bodies with atmosphere enabled
	std::vector<SBody *> vBodiesToRender;
	for(auto &Body : System.m_vBodies)
	{
		if(Body.m_RenderParams.m_Atmosphere.m_Enabled)
			vBodiesToRender.push_back(&Body);
	}

	// Sort by distance to camera (Far to Near) for correct transparency blending
	std::sort(vBodiesToRender.begin(), vBodiesToRender.end(), [&](SBody *a, SBody *b) {
		double distA = (a->m_SimParams.m_Position - Camera.m_AbsolutePosition).length();
		double distB = (b->m_SimParams.m_Position - Camera.m_AbsolutePosition).length();
		return distA > distB;
	});

	glm::mat4 gridView = glm::lookAt(glm::vec3(0.0f), (glm::vec3)Camera.m_Front, (glm::vec3)Camera.m_Up);
	glm::mat4 viewToWorld = glm::inverse(gridView);

	for(SBody *pBody : vBodiesToRender)
	{
		Quat q = pBody->m_SimParams.m_Orientation;

		// Pass Rotation Matrix for Shadow Map Lookup (Local -> World)
		glm::quat glmQ(q.w, q.x, q.y, q.z);
		glm::mat4 MatRot = glm::mat4_cast(glmQ);
		m_Shader.SetMat4("u_planetRotation", MatRot);

		// Pass Inverse for Ray Direction (World -> Local)
		Quat qInv = q.Conjugate();
		glm::quat glmQInv(qInv.w, qInv.x, qInv.y, qInv.z);
		glm::mat4 MatRotInv = glm::mat4_cast(glmQInv);

		glm::mat4 ViewToLocal = MatRotInv * viewToWorld;

		m_Shader.SetMat4("u_invView", ViewToLocal);
		m_Shader.SetMat4("u_invProjection", glm::inverse(Camera.m_Projection));

		double RealRadius = pBody->m_RenderParams.m_Radius;
		Vec3 RelativeCamPos = Camera.m_AbsolutePosition - pBody->m_SimParams.m_Position;
		Vec3 LocalCamPos = qInv.RotateVector(RelativeCamPos);
		Vec3 NormalizedCamPos = LocalCamPos / RealRadius;

		m_Shader.SetVec3("u_cameraPos", (glm::vec3)NormalizedCamPos);

		Vec3 SunVector = System.m_pSunBody->m_SimParams.m_Position - pBody->m_SimParams.m_Position;
		Vec3 SunDir;
		if(SunVector.length() > 1.0)
			SunDir = qInv.RotateVector(SunVector.normalize());
		else
			SunDir = Vec3(0, 1, 0);
		m_Shader.SetVec3("u_sunDirection", (glm::vec3)SunDir);

		m_Shader.SetFloat("u_realPlanetRadius", (float)RealRadius);
		m_Shader.SetFloat("u_sunIntensity", pBody->m_RenderParams.m_Atmosphere.m_SunIntensity);

		float atmRadMeters = pBody->m_RenderParams.m_Atmosphere.m_AtmosphereRadius * (float)RealRadius;
		m_Shader.SetFloat("u_atmosphereRadius", atmRadMeters);

		m_Shader.SetVec3("u_rayleighScatteringCoeff", pBody->m_RenderParams.m_Atmosphere.m_RayleighScatteringCoeff);
		m_Shader.SetFloat("u_rayleighScaleHeight", pBody->m_RenderParams.m_Atmosphere.m_RayleighScaleHeight);
		m_Shader.SetVec3("u_mieScatteringCoeff", pBody->m_RenderParams.m_Atmosphere.m_MieScatteringCoeff);
		m_Shader.SetFloat("u_mieScaleHeight", pBody->m_RenderParams.m_Atmosphere.m_MieScaleHeight);
		m_Shader.SetFloat("u_miePreferredScatteringDir", pBody->m_RenderParams.m_Atmosphere.m_MiePreferredScatteringDir);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}

	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void CAtmosphere::Destroy()
{
	m_Shader.Destroy();
}
