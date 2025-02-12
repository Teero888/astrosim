#include "gfx/graphics.h"
#include "gfx/trajectories.h"
#include "sim/starsystem.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>

int main()
{
	CStarSystem StarSystem;
	StarSystem.OnInit();

	CGraphics GfxEngine;
	if(!GfxEngine.OnInit(&StarSystem))
	{
		printf("Error while initializing graphics backend.\n");
		return 1;
	}

	// for(int i = 0; i < 1'000'000; ++i)
	// 	StarSystem.UpdateBodies();

	GfxEngine.m_Camera.m_pFocusedBody = &StarSystem.m_vBodies.front();

	// This is for the trajectories
	CStarSystem PredictedStarSystem = StarSystem;

	using namespace std::chrono;
	auto LastRenderTick = high_resolution_clock::now();
	double AccTime = 0.0;

	while(!glfwWindowShouldClose(GfxEngine.GetWindow()))
	{
		const double UpdateInterval = 1.0 / (StarSystem.m_DPS * (86400.0 / StarSystem.m_DeltaTime));
		glfwPollEvents();

		const auto CurrentTime = high_resolution_clock::now();
		double ElapsedTime = duration_cast<duration<double>>(CurrentTime - LastRenderTick).count();
		LastRenderTick = CurrentTime;
		AccTime += ElapsedTime;

		while(AccTime >= UpdateInterval)
		{
			StarSystem.UpdateBodies();
			AccTime -= UpdateInterval;
			while(PredictedStarSystem.m_SimTick < StarSystem.m_SimTick + TRAJECTORY_LENGTH)
			{
				GfxEngine.m_Trajectories.Update(PredictedStarSystem);
				PredictedStarSystem.UpdateBodies();
			}
		}
		GfxEngine.m_Camera.UpdateViewMatrix();
		GfxEngine.m_Trajectories.UpdateBuffers(PredictedStarSystem, GfxEngine.m_Camera);

		GfxEngine.OnRender();
	}
	GfxEngine.OnExit();
	return 0;
}
