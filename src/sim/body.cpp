#include "body.h"
#include "vmath.h"
#include <string>

SBody::SBody(int Id, const std::string &Name, double Mass, double Radius, Vec3 Position, Vec3 Velocity, glm::vec3 Color) :
	m_Name(Name), m_Mass(Mass), m_Radius(Radius), m_Position(Position), m_Velocity(Velocity), m_Acceleration(0, 0, 0), m_Color(Color), m_Id(Id) {}
