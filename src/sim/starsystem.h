#ifndef STARTSYSTEM_H
#define STARTSYSTEM_H

#include "body.h"
#include <cstdint>
#include <vector>

struct CStarSystem
{
	// basically the speed/accuracy of the simultion
	// 8.0 guarantees 180 tps even if days per sec is 1
	double m_DeltaTime = 60.0 * 8.0; // Time step in seconds

	uint64_t m_SimTick = 0;
	int m_TPS = 7;
	std::vector<SBody> m_vBodies;

	Vec3 m_LightSource;

	void RenderBody(SBody *pBody, SBody *pLightBody, class CShader *pShader, class CCamera *pCamera);

	void OnInit();
	void RenderBodies(class CShader *pShader, class CCamera *pCamera);
	void UpdateBodies();
};

#endif // STARTSYSTEM_H
