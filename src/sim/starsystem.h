#ifndef STARTSYSTEM_H
#define STARTSYSTEM_H

#include "body.h"
#include <cstdint>
#include <vector>

struct CStarSystem
{
	double m_DeltaTime = 60.0 * 8.0; // Time step in seconds
	uint64_t m_SimTick = 0;
	int m_DPS = 7; // Days per second
	std::vector<SBody> m_vBodies;
	SBody *m_pSunBody = nullptr;

	void OnInit();
	void LoadBodies(const std::string &filename);
	void UpdateBodies();
};

#endif // STARTSYSTEM_H
