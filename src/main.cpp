#define GLM_FORCE_DEFAULT_PRECISION_DOUBLE
#include "gfx/graphics.h"
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

	// for(int i = 0; i < 1'000'000; ++i)
	// {
	// 	StarSystem.UpdateBodies();
	// 	GfxEngine.m_Camera.UpdateViewMatrix();
	// }

	GfxEngine.m_Camera.m_pFocusedBody = &StarSystem.m_vBodies.front();

	// constexpr int PredictionLength = 3600;
	// CStarSystem PredictedStarSystem = StarSystem;

	using namespace std::chrono;
	auto LastTick = high_resolution_clock::now();
	auto LastRenderTick = high_resolution_clock::now();
	double accumulatedTime = 0.0;

	while(!glfwWindowShouldClose(GfxEngine.GetWindow()))
	{
		const double updateInterval = 1.0 / StarSystem.m_TPS;
		glfwPollEvents();

		auto currentTime = high_resolution_clock::now();
		double elapsedTime = duration_cast<duration<double>>(currentTime - LastRenderTick).count();
		LastRenderTick = currentTime;
		accumulatedTime += elapsedTime;

		while(accumulatedTime >= updateInterval)
		{
			StarSystem.UpdateBodies();
			GfxEngine.m_Camera.UpdateViewMatrix();
			accumulatedTime -= updateInterval;
			// while(PredictedStarSystem.m_SimTick < StarSystem.m_SimTick + PredictionLength)
			// {
			// 	PredictedStarSystem.UpdateBodies();
			// }
		}

		GfxEngine.OnRender();
	}
	GfxEngine.OnExit();
	return 0;
}
