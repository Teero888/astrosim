#include "starsystem.h"
#include "body.h"
#include "vmath.h"
#include <chrono>

void CStarSystem::OnInit()
{
	*this = CStarSystem();
	LoadBodies("data/bodies.toml");
}

void CStarSystem::UpdateBodies()
{
	const size_t BodyCount = m_vBodies.size();

	const double HalfDt = 0.5 * m_DeltaTime;
	for(size_t i = 0; i < BodyCount; ++i)
	{
		auto &Params = m_vBodies[i].m_SimParams;
		Params.m_Velocity += Params.m_Acceleration * HalfDt;
		Params.m_Position += Params.m_Velocity * m_DeltaTime;
		Params.m_Acceleration = Vec3(0.0);
	}

	for(size_t i = 0; i < BodyCount; ++i)
	{
		auto &BodyA = m_vBodies[i].m_SimParams;
		for(size_t j = i + 1; j < BodyCount; ++j)
		{
			auto &BodyB = m_vBodies[j].m_SimParams;
			Vec3 r = BodyB.m_Position - BodyA.m_Position;
			double DistSq = r.dot(r);
			double Dist = std::sqrt(DistSq);
			double DistCubed = DistSq * Dist;
			double CommonFactor = G / DistCubed;
			Vec3 ForceVec = r * CommonFactor;
			BodyA.m_Acceleration += ForceVec * BodyB.m_Mass;
			BodyB.m_Acceleration -= ForceVec * BodyA.m_Mass;
		}
	}

	for(size_t i = 0; i < BodyCount; ++i)
	{
		auto &Params = m_vBodies[i].m_SimParams;
		Params.m_Velocity += Params.m_Acceleration * HalfDt;
		IntegrateRotation(Params.m_Orientation, Params.m_AngularVelocity, m_DeltaTime);
	}

	++m_SimTick;
}

int CStarSystem::Benchmark()
{
	using namespace std::chrono;
	auto Start = high_resolution_clock::now();

	for(int i = 0; i < 1e6; ++i)
		UpdateBodies();

	auto End = high_resolution_clock::now();
	return 1e6 / (high_resolution_clock::duration(End - Start).count() / 1e9);
}
