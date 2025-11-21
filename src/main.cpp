#include "gfx/graphics.h"
#include "gfx/trajectories.h"
#include "sim/starsystem.h"
#include <GLFW/glfw3.h>
#include <chrono>
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

	GfxEngine.m_Camera.SetBody(&StarSystem.m_vBodies.front());

	// This is for the trajectories
	CStarSystem PredictedStarSystem = StarSystem;

	using namespace std::chrono;
	auto LastRenderTick = high_resolution_clock::now();
	double AccTime = 0.0;

	while(!glfwWindowShouldClose(GfxEngine.GetWindow()))
	{
		const double UpdateInterval = 1.0 / (StarSystem.m_HPS * (3600.0 / StarSystem.m_DeltaTime));

		glfwPollEvents();

		if(GfxEngine.m_bReloadRequested)
		{
			GfxEngine.ReloadSimulation();
			PredictedStarSystem = StarSystem;
			GfxEngine.m_bReloadRequested = false;
		}

		if(GfxEngine.m_bPredictionResetRequested)
		{
			PredictedStarSystem = StarSystem;
			GfxEngine.m_bPredictionResetRequested = false;
		}

		const auto CurrentTime = high_resolution_clock::now();
		double ElapsedTime = duration_cast<duration<double>>(CurrentTime - LastRenderTick).count();
		LastRenderTick = CurrentTime;
		AccTime += ElapsedTime;

		if(GfxEngine.m_bIsRunning)
		{
			while(AccTime >= UpdateInterval)
			{
				StarSystem.UpdateBodies();
				AccTime -= UpdateInterval;
				uint64_t Horizon = (uint64_t)GfxEngine.m_Trajectories.m_PredictionDuration;
				uint64_t TargetTick = StarSystem.m_SimTick + Horizon;
				while(PredictedStarSystem.m_SimTick < TargetTick)
				{
					GfxEngine.m_Trajectories.Update(PredictedStarSystem);
					PredictedStarSystem.UpdateBodies();
				}
			}
		}
		else
			AccTime = 0.0;

		GfxEngine.m_Camera.UpdateViewMatrix();
		GfxEngine.m_Trajectories.UpdateBuffers(StarSystem, PredictedStarSystem, GfxEngine.m_Camera);

		GfxEngine.OnRender(StarSystem);
	}

	GfxEngine.OnExit();
	return 0;
}
