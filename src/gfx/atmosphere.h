#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H
#include "shader.h"

class CStarSystem;
struct SBody;
class CCamera;

class CAtmosphere
{
	CShader m_Shader;

	// == Transmittance LUT Resources ==
	CShader m_BakerShader;
	unsigned int m_LUT_FBO = 0;
	unsigned int m_LUT_Texture = 0;

	// To detect when to re-bake
	int m_LastBakedBodyId = -1;

public:
	void Init();
	void BakeLUT(const SBody &Body);
	void Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit);
	void Destroy();
};
#endif // ATMOSPHERE_H
