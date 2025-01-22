#ifndef TRAJECTORIES_H
#define TRAJECTORIES_H

#include "../sim/starsystem.h"
#include "../sim/vmath.h"
#include "shader.h"
#include <vector>

class CTrajectories
{
	struct STrajectory
	{
		glm::vec3 m_Color;
		std::vector<Vec3> m_vPositionHistory;
		// i know this duplicate sounds bad but it's neccessary
		std::vector<glm::vec3> m_vGLPositions; // float version for OpenGL
		GLuint VAO, VBO; // opengl buffers
		float m_LineWidth = 1.0f;
	};
	CShader m_Shader;
	std::vector<STrajectory> m_vPlanetTrajectories;

public:
	int m_TrajectoryLength = 36000;
	void Init();
	void Update(CStarSystem &PredictedSystem);
	void UpdateBuffers(CStarSystem &PredictedSystem, CCamera &Camera);
	void Render(CCamera *pCamera);
	void Destroy();
};

#endif // TRAJECTORIES_H
