#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H
#include "shader.h"

struct CStarSystem;
struct SBody;
struct CCamera;

class CAtmosphere
{
	CShader m_Shader;

public:
	void Init();
	void BakeLUT(const SBody &Body);
	void Render(CStarSystem &System, CCamera &Camera, unsigned int DepthTextureUnit, unsigned int ShadowTextureID, const glm::mat4 &LightSpaceMatrix, int DebugMode);
	void Destroy();
};
#endif // ATMOSPHERE_H
