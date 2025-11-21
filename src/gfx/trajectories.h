#ifndef TRAJECTORIES_H
#define TRAJECTORIES_H

#include "../sim/starsystem.h"
#include "shader.h"
#include <glm/glm.hpp>
#include <vector>

struct CStarSystem;
struct CCamera;

class CTrajectories
{
	struct STrajectory
	{
		glm::vec3 m_Color;
		std::vector<Vec3> m_PositionHistory;
		std::vector<glm::vec3> m_GLHistory;
		GLuint VAO = 0, VBO = 0;
		float m_LineWidth = 2.0f;
		int m_PointCount = 0;
	};
	CShader m_Shader;
	std::vector<STrajectory> m_vPlanetTrajectories;

public:
	int m_PredictionDuration = 200000; // in ticks
	int m_SampleRate = 2500;
	int GetMaxVisualPoints() const { return (m_SampleRate > 0) ? (m_PredictionDuration / m_SampleRate) : 0; }

	bool m_Show = true;
	void Init();
	void Update(CStarSystem &PredictedSystem);
	void UpdateBuffers(CStarSystem &RealTimeSystem, CStarSystem &PredictedSystem, CCamera &Camera);

	void Render(CCamera &Camera);
	void Destroy();
	void ClearTrajectories();
};

#endif // TRAJECTORIES_H
