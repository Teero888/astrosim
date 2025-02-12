#ifndef BODY_H
#define BODY_H

#include "vmath.h"
#include <glm/gtc/type_ptr.hpp>
#include <string>

struct SBody
{
	std::string m_Name;
	int m_Id;

	// clang-format off
	struct SSimParams
	{
		double m_Mass;			// Mass (kg)
		Vec3   m_Position;		// Position (m)
		Vec3   m_Velocity;		// Velocity (m/s)
		Vec3   m_Acceleration;	// Acceleration (m/s^2)
	} m_SimParams;

	struct SRenderParams
	{
		double m_Radius; // Planetary radius [meters] (Earth ≈ 6.371e6 m)
	} m_RenderParams;
	// clang-format on

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H
