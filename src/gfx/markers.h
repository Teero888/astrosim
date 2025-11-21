#ifndef MARKERS_H
#define MARKERS_H

#include "shader.h"
class CMarkers
{
	CShader m_Shader;

public:
	bool m_ShowMarkers = true;
	void Init();
	void Render(struct CStarSystem &System, struct CCamera &Camera);
	void Destroy();
};

#endif // MARKERS_H
