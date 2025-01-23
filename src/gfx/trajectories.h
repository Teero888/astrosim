#ifndef TRAJECTORIES_H
#define TRAJECTORIES_H

#include "../sim/starsystem.h"
#include "shader.h"
#include <vector>

class CTrajectories
{
	struct STrajectory
	{
		glm::vec3 m_Color;
		std::vector<glm::vec3> m_vPositionHistory;
		GLuint VAO, VBO;
		float m_LineWidth = 2.0f;
	};
	CShader m_Shader;
	std::vector<STrajectory> m_vPlanetTrajectories;

public:
	bool m_ShowAll = false;
	int m_TrajectoryLength = 3600;
	void Init();
	void Update(CStarSystem &PredictedSystem);
	void UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera);
	void Render(CCamera &Camera);
	void Destroy();
};

#endif // TRAJECTORIES_H
