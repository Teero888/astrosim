#ifndef STARTSYSTEM_H
#define STARTSYSTEM_H

#include "body.h"
#include <cstdint>
#include <vector>

constexpr double G = 6.67430e-11;
constexpr double PI = 3.14159265358979323846;

struct CStarSystem
{
	double m_DeltaTime = 1.0; // Time step in minutes
	uint64_t m_SimTick = 0;
	float m_HPS = 1; // Hours per second
	std::vector<SBody> m_vBodies;
	SBody *m_pSunBody = nullptr;

	void OnInit();
	void LoadBodies(const std::string &filename);
	void UpdateBodies();
};

#endif // STARTSYSTEM_H
