#ifndef TRAJECTORIES_H
#define TRAJECTORIES_H

#include "../sim/starsystem.h"
#include "shader.h"
#include <cstring>
#include <vector>

#define TRAJECTORY_LENGTH 3600

class CStarSystem;
class CCamera;
class CTrajectories
{
	struct STrajectory
	{
		glm::vec3 m_Color;
		Vec3 m_aPositionHistory[TRAJECTORY_LENGTH];
		glm::vec3 m_aGLHistory[TRAJECTORY_LENGTH];
		GLuint VAO, VBO;
		float m_LineWidth = 2.0f;
		STrajectory()
		{
			memset(m_aPositionHistory, 0, TRAJECTORY_LENGTH * sizeof(Vec3));
			memset(m_aGLHistory, 0, TRAJECTORY_LENGTH * sizeof(glm::vec3));
		};
	};
	CShader m_Shader;
	std::vector<STrajectory> m_vPlanetTrajectories;

public:
	bool m_ShowAll = true;
	void Init();
	void Update(CStarSystem &PredictedSystem);
	void UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera);
	void Render(CCamera &Camera);
	void Destroy();
	void ClearTrajectories();
};

#endif // TRAJECTORIES_H
