#ifndef BODY_H
#define BODY_H

#include "vmath.h"
#include <glm/gtc/type_ptr.hpp>
#include <string>

struct SBody
{
	std::string m_Name;
	double m_Mass; // Mass (kg)
	double m_Radius; // Radius (m)
	Vec3 m_Position; // Position (m)
	Vec3 m_Velocity; // Velocity (m/s)
	Vec3 m_Acceleration; // Acceleration (m/s^2)
	glm::vec3 m_Color;

	SBody(const std::string &Name, double Mass, double Radius, Vec3 Position, Vec3 Velocity, glm::vec3 Color);
};

#endif // BODY_H
