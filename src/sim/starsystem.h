#ifndef STARTSYSTEM_H
#define STARTSYSTEM_H

#include "body.h"
#include <cstdint>
#include <vector>

struct CStarSystem
{
	uint64_t m_SimTick = 0;
	int m_TPS = 24; // dt is 1hr is 1 tick so this should be 1 day/s
	std::vector<SBody> m_vBodies;

	Vec3 m_LightSource;

	void RenderBody(SBody *pBody, SBody *pLightBody, class CShader *pShader, class CCamera *pCamera);

	void OnInit();
	void RenderBodies(class CShader *pShader, class CCamera *pCamera);
	void UpdateBodies();
};

#endif // STARTSYSTEM_H
