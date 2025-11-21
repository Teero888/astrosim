#include "starsystem.h"
#include "body.h"
#include "vmath.h"

void CStarSystem::OnInit()
{
	*this = CStarSystem();
	LoadBodies("data/bodies.dat");
}

static Vec3 CalculateForce(const SBody &a, const SBody &b)
{
	Vec3 r = b.m_SimParams.m_Position - a.m_SimParams.m_Position;
	const double Distance = r.length();
	if(Distance < 1e-3)
		return Vec3(0);
	const double ForceMagnitude = (G * a.m_SimParams.m_Mass * b.m_SimParams.m_Mass) / (Distance * Distance);
	Vec3 Force = r * (ForceMagnitude / Distance);
	return Force;
}

void CStarSystem::UpdateBodies()
{
	if(m_vBodies.empty())
		return;

	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);

	for(auto &Body : m_vBodies)
		Body.m_SimParams.m_Position = Body.m_SimParams.m_Position + Body.m_SimParams.m_Velocity * m_DeltaTime;

	for(auto &Body : m_vBodies)
	{
		Vec3 TotalForce(0, 0, 0);
		for(const auto &other : m_vBodies)
			if(&Body != &other)
				TotalForce = TotalForce + CalculateForce(Body, other);
		Body.m_SimParams.m_Acceleration = TotalForce / Body.m_SimParams.m_Mass;
	}

	for(auto &Body : m_vBodies)
	{
		Body.m_SimParams.m_Velocity = Body.m_SimParams.m_Velocity + Body.m_SimParams.m_Acceleration * (0.5 * m_DeltaTime);
		IntegrateRotation(Body.m_SimParams.m_Orientation, Body.m_SimParams.m_AngularVelocity, m_DeltaTime);
	}

	++m_SimTick;
}