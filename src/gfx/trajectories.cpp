#include "trajectories.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/vector_float3.hpp"
#include <cstdio>

void CTrajectories::Init()
{
	m_Shader.CompileShader(Shaders::VERT_GRID, Shaders::FRAG_TRAJECTORY);
}

void CTrajectories::Update(CStarSystem &PredictedSystem)
{
	while(m_vPlanetTrajectories.size() < PredictedSystem.m_vBodies.size())
	{
		STrajectory NewTraj;
		NewTraj.m_Color = glm::vec3(1.0);
		glGenVertexArrays(1, &NewTraj.VAO);
		glGenBuffers(1, &NewTraj.VBO);
		m_vPlanetTrajectories.emplace_back(NewTraj);
	}

	for(size_t i = 0; i < PredictedSystem.m_vBodies.size(); ++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];
		auto &Body = PredictedSystem.m_vBodies[i];

		Trajectory.m_vPositionHistory.emplace_back(Body.m_Position);

		if(Trajectory.m_vPositionHistory.size() > m_TrajectoryLength)
			Trajectory.m_vPositionHistory.erase(Trajectory.m_vPositionHistory.begin());
	}
}

void CTrajectories::UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera)
{
	if(m_vPlanetTrajectories.empty())
		return;
	for(size_t i = 0; i < PredictedSystem.m_vBodies.size(); ++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];

		Trajectory.m_vGLPositions.clear(); // .reserve is not needed since clear doesn't clear the memory afaik
		for(const auto &Pos : Trajectory.m_vPositionHistory)
		{
			Trajectory.m_vGLPositions.emplace_back((Pos - m_vPlanetTrajectories[Camera.m_pFocusedBody->m_Id].m_vPositionHistory.front()) / RENDER_SCALE);
			// if(i == 1)
			// 	printf("Pos:%.4f, %.4f, %.4f\n", Trajectory.m_vGLPositions.back().x, Trajectory.m_vGLPositions.back().y, Trajectory.m_vGLPositions.back().z);
		}

		// update gpu buffers
		glBindVertexArray(Trajectory.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, Trajectory.VBO);
		glBufferData(GL_ARRAY_BUFFER,
			Trajectory.m_vGLPositions.size() * sizeof(glm::vec3),
			Trajectory.m_vGLPositions.data(),
			GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
		glEnableVertexAttribArray(0);

		glBindVertexArray(0);
	}
}

void CTrajectories::Render(CCamera *pCamera)
{
	m_Shader.Use();
	// m_Shader.SetMat4("ViewProjection", pCamera->m_View * pCamera->m_Projection);

	m_Shader.SetFloat("Scale", 1.0);
	glm::mat4 model = glm::mat4(1.0f);
	// model = glm::translate(model, (glm::vec3)((pCamera->m_FocusPoint - pCamera->m_pFocusedBody->m_Position) / RENDER_SCALE));
	m_Shader.SetMat4("Model", model);
	m_Shader.SetMat4("View", pCamera->m_View);
	m_Shader.SetMat4("Projection", pCamera->m_Projection);
	glDisable(GL_DEPTH_TEST); // for always visible trajectories
	glEnable(GL_LINE_SMOOTH);

	for(auto &Trajectory : m_vPlanetTrajectories)
	{
		if(Trajectory.m_vPositionHistory.size() < 2)
			continue;

		m_Shader.SetVec3("Color", Trajectory.m_Color);
		glLineWidth(Trajectory.m_LineWidth);

		glBindVertexArray(Trajectory.VAO);
		glDrawArrays(GL_LINE_STRIP, 0, Trajectory.m_vPositionHistory.size());
		glBindVertexArray(0);
	}

	glEnable(GL_DEPTH_TEST);
}

void CTrajectories::Destroy()
{
	for(auto &trajectory : m_vPlanetTrajectories)
	{
		glDeleteVertexArrays(1, &trajectory.VAO);
		glDeleteBuffers(1, &trajectory.VBO);
	}
	m_Shader.Destroy();
}
