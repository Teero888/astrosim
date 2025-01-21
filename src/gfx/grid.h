#ifndef GRID_H
#define GRID_H
#include "shader.h"
#include <cmath>

class CCamera;
class CGrid
{
	CShader m_Shader;

public:
	void Init();
	void Render(CCamera *pCamera);
	void Destroy();
};
#endif // GRID_H
