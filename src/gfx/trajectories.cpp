#include "trajectories.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "shader.h"
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

	for(size_t i = 0; i < PredictedSystem.m_vBodies.size(); ++i)
		m_vPlanetTrajectories[i].m_aPositionHistory[PredictedSystem.m_SimTick % TRAJECTORY_LENGTH] = PredictedSystem.m_vBodies[i].m_Position;
}

void CTrajectories::UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera)
{
	if(m_vPlanetTrajectories.empty())
		return;

	for(int i = m_ShowAll ? 0 : Camera.m_pFocusedBody->m_Id;
		m_ShowAll ? i < (int)m_vPlanetTrajectories.size() : i == Camera.m_pFocusedBody->m_Id;
		++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];
		for(int i = 0; i < TRAJECTORY_LENGTH; ++i)
		{
			Vec3 NewPos = (Trajectory.m_aPositionHistory[i] - Camera.m_pFocusedBody->m_Position) / Camera.m_Radius;
			Trajectory.m_aGLHistory[i] = NewPos;
			// 0,0,0 is reserved for not rendering so make it some val that is not 0,0,0
			if(Trajectory.m_aGLHistory[i] == glm::vec3(0.f))
				Trajectory.m_aGLHistory[i] = glm::vec3(0.01f);
		}

		Trajectory.m_aGLHistory[(PredictedSystem.m_SimTick - 1) % TRAJECTORY_LENGTH] = {};

		// update gpu buffers
		glBindVertexArray(Trajectory.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, Trajectory.VBO);
		glBufferData(GL_ARRAY_BUFFER,
			TRAJECTORY_LENGTH * sizeof(glm::vec3),
			Trajectory.m_aGLHistory,
			GL_STATIC_DRAW);

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
	// Model = glm::translate(Model, -(glm::vec3)(Camera.m_pFocusedBody->m_Position / (double)Camera.m_Radius));
	// Model = glm::scale(Model, glm::vec3(1.0 / (double)Camera.m_Radius));
	m_Shader.SetMat4("Model", Model);
	m_Shader.SetMat4("View", Camera.m_View);
	m_Shader.SetMat4("Projection", Camera.m_Projection);
	glDisable(GL_DEPTH_TEST); // for always visible trajectories
	glEnable(GL_LINE_SMOOTH);

	for(int i = m_ShowAll ? 0 : Camera.m_pFocusedBody->m_Id;
		m_ShowAll ? i < (int)m_vPlanetTrajectories.size() : i == Camera.m_pFocusedBody->m_Id;
		++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];
		m_Shader.SetVec3("Color", Trajectory.m_Color);
		glLineWidth(Trajectory.m_LineWidth);

		glBindVertexArray(Trajectory.VAO);
		glDrawArrays(GL_LINE_STRIP, 0, TRAJECTORY_LENGTH);
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
