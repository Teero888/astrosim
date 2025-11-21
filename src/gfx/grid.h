#ifndef GRID_H
#define GRID_H
#include "shader.h"
#include <cmath>

struct CCamera;
class CGrid
{
	// struct SPlanetInfo
	// {
	// 	float Scale;
	// 	float Mass;
	// 	float Position[3]; // vec3 in GLSL
	// };
	// std::vector<SPlanetInfo> m_vPlanetInfo;
	// GLuint m_PlanetSSBO;
	CShader m_Shader;

public:
	void Init();
	void Render(struct CStarSystem &System, CCamera &Camera);
	void Destroy();
};
#endif // GRID_H
