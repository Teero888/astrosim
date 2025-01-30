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
		double m_Radius;				// Planetary radius [meters] (Earth ≈ 6.371e6 m)
		float  m_Albedo;				// Diffuse reflectivity [0.0-1.0] (Moon=0.12, Earth=0.3)
		float  m_Roughness;				// Surface micro-roughness [0.0(smooth)-1.0(rough)]
		float  m_Specularity;			// Reflectivity intensity [0.0-1.0] (Water=0.8, Rock=0.05)
		glm::vec3 m_SurfaceColor;		// Base RGB color of terrain [0.0-1.0]
		glm::vec3 m_OceanColor;			// RGB color of liquid surface [0.0-1.0]
		glm::vec3 m_CloudColor;			// RGB color of clouds [0.0-1.0]
		glm::vec3 m_AtmoColor;			// RGB atmospheric tint [0.0-1.0]
		float  m_AtmoStrength;			// Atmospheric density [0.0(none)-2.0(thick)]
		float  m_CloudCover;			// Cloud coverage [0.0(clear)-1.0(overcast)]
		float  m_CoreTemperature;		// Internal heat [°C] (Sun=15M°C, Earth=6000°C)
		float  m_SurfaceTemperature;	// Average surface temp [°C] (Earth=15°C)
		float  m_OrbitalSpeed;			// Orbital velocity [m/s] (Earth≈29,780 m/s)
		float  m_RotationSpeed;			// Axial rotation [radians/sec] (Earth=7.29e-5 rad/s)
		float  m_MagneticField;			// Magnetic field strength [Tesla] (Earth≈25-65μT)
		float  m_AuroraIntensity;		// Polar light strength [0.0(none)-1.0(max)]
	} m_RenderParams;
	// clang-format on

	SBody(int Id, const std::string &Name, SSimParams SimulationParameters, SRenderParams RenderParameters);
};

#endif // BODY_H
