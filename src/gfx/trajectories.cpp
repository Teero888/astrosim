#include "trajectories.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include <cstdio>

void CTrajectories::Init()
{
	m_Shader.CompileShader(Shaders::VERT_TRAJECTORY, Shaders::FRAG_TRAJECTORY);
}

void CTrajectories::Update(CStarSystem &PredictedSystem)
{
	if(m_vPlanetTrajectories.empty())
	{
		m_vPlanetTrajectories.resize(PredictedSystem.m_vBodies.size());
		for(int i = 0; i < (int)m_vPlanetTrajectories.size(); ++i)
		{
			auto &Traj = m_vPlanetTrajectories[i];
			Traj.m_Color = PredictedSystem.m_vBodies[i].m_Color;
			glGenVertexArrays(1, &Traj.VAO);
			glGenBuffers(1, &Traj.VBO);
		}
	}

	constexpr int Interval = 1;
	if(PredictedSystem.m_SimTick % Interval == 0)
		for(size_t i = 0; i < PredictedSystem.m_vBodies.size(); ++i)
		{
			auto &Trajectory = m_vPlanetTrajectories[i];

			Trajectory.m_vPositionHistory.emplace_back(PredictedSystem.m_vBodies[i].m_Position);

			while(Trajectory.m_vPositionHistory.size() > m_TrajectoryLength / Interval)
				Trajectory.m_vPositionHistory.erase(Trajectory.m_vPositionHistory.begin());
		}
}

void CTrajectories::UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera)
{
	if(m_vPlanetTrajectories.empty())
		return;
	if(m_ShowAll)
	{
		for(auto &Trajectory : m_vPlanetTrajectories)
		{
			// update gpu buffers
			glBindVertexArray(Trajectory.VAO);
			glBindBuffer(GL_ARRAY_BUFFER, Trajectory.VBO);
			glBufferData(GL_ARRAY_BUFFER,
				Trajectory.m_vPositionHistory.size() * sizeof(glm::vec3),
				Trajectory.m_vPositionHistory.data(),
				GL_DYNAMIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
			glEnableVertexAttribArray(0);

			glBindVertexArray(0);
		}
	}
	else
	{
		auto &Trajectory = m_vPlanetTrajectories[Camera.m_pFocusedBody->m_Id];

		// update gpu buffers
		glBindVertexArray(Trajectory.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, Trajectory.VBO);
		glBufferData(GL_ARRAY_BUFFER,
			Trajectory.m_vPositionHistory.size() * sizeof(glm::vec3),
			Trajectory.m_vPositionHistory.data(),
			GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
		glEnableVertexAttribArray(0);

		glBindVertexArray(0);
	}
}

void CTrajectories::Render(CCamera &Camera)
{
	if(m_vPlanetTrajectories.empty())
		return;
	m_Shader.Use();
	auto Model = glm::mat4(1.0f);
	Model = glm::translate(Model, -(glm::vec3)(Camera.m_pFocusedBody->m_Position / (double)Camera.m_Radius));
	Model = glm::scale(Model, glm::vec3(1.0 / (double)Camera.m_Radius));
	m_Shader.SetMat4("Model", Model);
	m_Shader.SetMat4("View", Camera.m_View);
	m_Shader.SetMat4("Projection", Camera.m_Projection);
	glDisable(GL_DEPTH_TEST); // for always visible trajectories
	glEnable(GL_LINE_SMOOTH);

	if(m_ShowAll)
	{
		for(auto &Trajectory : m_vPlanetTrajectories)
			if(Trajectory.m_vPositionHistory.size() >= 2)
			{
				m_Shader.SetVec3("Color", Trajectory.m_Color);
				glLineWidth(Trajectory.m_LineWidth);

				glBindVertexArray(Trajectory.VAO);
				glDrawArrays(GL_LINE_STRIP, 0, Trajectory.m_vPositionHistory.size());
				glBindVertexArray(0);
			}
	}
	else
	{
		auto &Trajectory = m_vPlanetTrajectories[Camera.m_pFocusedBody->m_Id];
		if(Trajectory.m_vPositionHistory.size() >= 2)
		{
			m_Shader.SetVec3("Color", Trajectory.m_Color);
			glLineWidth(Trajectory.m_LineWidth);

			glBindVertexArray(Trajectory.VAO);
			glDrawArrays(GL_LINE_STRIP, 0, Trajectory.m_vPositionHistory.size());
			glBindVertexArray(0);
		}
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
