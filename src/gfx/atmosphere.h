#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H
#include "shader.h"

class CStarSystem;
class CCamera;

class CAtmosphere
{
	CShader m_Shader;

public:
	void Init();
	// Update: Added DepthTextureUnit argument
	void Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit);
	void Destroy();
};
#endif // ATMOSPHERE_H
