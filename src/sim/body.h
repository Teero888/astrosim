#ifndef BODY_H
#define BODY_H

#include "vmath.h"
#include <string>

struct SBody
{
	std::string m_Name;
	double m_Mass; // Mass (kg)
	Vec3 m_Position; // Position (m)
	Vec3 m_Velocity; // Velocity (m/s)
	Vec3 m_Acceleration; // Acceleration (m/s^2)

	SBody(const std::string &name, double mass, Vec3 position, Vec3 velocity) :
		m_Name(name), m_Mass(mass), m_Position(position), m_Velocity(velocity), m_Acceleration(0, 0, 0) {}

	void Render(class CShader *pShader, class CCamera *pCamera);
};

#endif // BODY_H
