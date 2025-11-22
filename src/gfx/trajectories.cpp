#include "trajectories.h"
#include "camera.h"
#include "glm/ext/vector_float3.hpp"
#include "shader.h"
#include <cstdio>
#include <embedded_shaders.h>

void CTrajectories::Init()
{
	m_Shader.CompileShader(Shaders::VERT_TRAJECTORY, Shaders::FRAG_TRAJECTORY);
}

void CTrajectories::Update(CStarSystem &PredictedSystem)
{
	if(m_SampleRate <= 0)
		return;

	if(PredictedSystem.m_SimTick % m_SampleRate != 0)
		return;

	int MaxPoints = GetMaxVisualPoints();
	if(MaxPoints < 2)
		return;

	if(m_vPlanetTrajectories.empty())
	{
		m_vPlanetTrajectories.resize(PredictedSystem.m_vBodies.size());
		for(int i = 0; i < (int)m_vPlanetTrajectories.size(); ++i)
		{
			auto &Traj = m_vPlanetTrajectories[i];
			Traj.m_Color = PredictedSystem.m_vBodies[i].m_RenderParams.m_Color;
			glGenVertexArrays(1, &Traj.VAO);
			glGenBuffers(1, &Traj.VBO);
			Traj.m_PointCount = 0;
		}
	}

	uint64_t VisualTick = PredictedSystem.m_SimTick / m_SampleRate;
	size_t BufferIndex = VisualTick % MaxPoints;

	for(size_t i = 0; i < PredictedSystem.m_vBodies.size(); ++i)
	{
		auto &Traj = m_vPlanetTrajectories[i];

		if((int)Traj.m_PositionHistory.size() != MaxPoints)
		{
			Traj.m_PositionHistory.resize(MaxPoints);
			Traj.m_GLHistory.resize(MaxPoints + 2);
			Traj.m_PointCount = 0;
			glBindBuffer(GL_ARRAY_BUFFER, Traj.VBO);
			glBufferData(GL_ARRAY_BUFFER, (MaxPoints + 2) * sizeof(glm::vec3), nullptr, GL_STREAM_DRAW);
		}

		Traj.m_PositionHistory[BufferIndex] = PredictedSystem.m_vBodies[i].m_SimParams.m_Position;

		if(Traj.m_PointCount < MaxPoints)
			Traj.m_PointCount++;
	}
}

void CTrajectories::UpdateBuffers(CStarSystem &RealTimeSystem, CStarSystem &PredictedSystem, CCamera &Camera)
{
	if(m_vPlanetTrajectories.empty() || !m_Show || m_SampleRate <= 0)
		return;

	int maxPoints = GetMaxVisualPoints();
	if(maxPoints < 2)
		return;

	uint64_t SimTick = PredictedSystem.m_SimTick;
	if(SimTick == 0)
		return;

	uint64_t VisualTick = (SimTick - 1) / m_SampleRate;
	size_t HeadIndex = VisualTick % maxPoints;

	for(int i = 0; i < (int)m_vPlanetTrajectories.size(); ++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];

		if((int)Trajectory.m_PositionHistory.size() != maxPoints)
			continue;
		if((int)Trajectory.m_GLHistory.size() != maxPoints + 2)
			continue;

		int PointsToDraw = Trajectory.m_PointCount + 2; // Start + Samples + End

		Vec3 StartPos = RealTimeSystem.m_vBodies[i].m_SimParams.m_Position;
		Trajectory.m_GLHistory[0] = (glm::vec3)(StartPos - Camera.m_AbsolutePosition);
		if(Trajectory.m_GLHistory[0] == glm::vec3(0.0f))
			Trajectory.m_GLHistory[0] = glm::vec3(0.0001f);

		if(Trajectory.m_PointCount > 0)
		{
			size_t TailIndex = (HeadIndex - Trajectory.m_PointCount + 1 + maxPoints) % maxPoints;
			for(int j = 0; j < Trajectory.m_PointCount; ++j)
			{
				size_t RingIndex = (TailIndex + j) % maxPoints;
				Vec3 WorldPos = Trajectory.m_PositionHistory[RingIndex];

				Vec3 RelPos = WorldPos - Camera.m_AbsolutePosition;
				Trajectory.m_GLHistory[j + 1] = (glm::vec3)RelPos;
				if(Trajectory.m_GLHistory[j + 1] == glm::vec3(0.0f))
					Trajectory.m_GLHistory[j + 1] = glm::vec3(0.0001f);
			}
		}

		Vec3 EndPos = PredictedSystem.m_vBodies[i].m_SimParams.m_Position;
		Trajectory.m_GLHistory[PointsToDraw - 1] = (glm::vec3)(EndPos - Camera.m_AbsolutePosition);
		if(Trajectory.m_GLHistory[PointsToDraw - 1] == glm::vec3(0.0f))
			Trajectory.m_GLHistory[PointsToDraw - 1] = glm::vec3(0.0001f);

		glBindVertexArray(Trajectory.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, Trajectory.VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, PointsToDraw * sizeof(glm::vec3), Trajectory.m_GLHistory.data());

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

	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	auto Model = glm::mat4(1.0f);
	m_Shader.SetMat4("Model", Model);
	m_Shader.SetMat4("View", Camera.m_View);
	m_Shader.SetMat4("Projection", Camera.m_Projection);
	glEnable(GL_LINE_SMOOTH);

	for(int i = m_Show ? 0 : Camera.m_pFocusedBody->m_Id;
		m_Show ? i < (int)m_vPlanetTrajectories.size() : i == Camera.m_pFocusedBody->m_Id;
		++i)
	{
		auto &Trajectory = m_vPlanetTrajectories[i];
		if(Trajectory.VAO == 0)
			continue;

		int count = Trajectory.m_PointCount + 2;
		if(count < 2)
			continue;

		m_Shader.SetVec3("Color", Trajectory.m_Color);
		glLineWidth(Trajectory.m_LineWidth);

		glBindVertexArray(Trajectory.VAO);
		glDrawArrays(GL_LINE_STRIP, 0, count);
		glBindVertexArray(0);
	}
}

void CTrajectories::Destroy()
{
	ClearTrajectories();
	m_Shader.Destroy();
}

void CTrajectories::ClearTrajectories()
{
	for(auto &trajectory : m_vPlanetTrajectories)
	{
		if(trajectory.VAO)
			glDeleteVertexArrays(1, &trajectory.VAO);
		if(trajectory.VBO)
			glDeleteBuffers(1, &trajectory.VBO);
	}
	m_vPlanetTrajectories.clear();
}
