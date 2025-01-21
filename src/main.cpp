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

	GfxEngine.m_Camera.m_pFocusedBody = &StarSystem.m_vBodies.back();

	using namespace std::chrono;
	auto LastTick = high_resolution_clock::now();

	while(!glfwWindowShouldClose(GfxEngine.GetWindow()))
	{
		glfwPollEvents();
		GfxEngine.OnRender();
		if(duration_cast<milliseconds>(high_resolution_clock::now() - LastTick).count() > 1000.0 / StarSystem.GetTPS())
		{
			StarSystem.UpdateBodies();
			GfxEngine.m_Camera.UpdateViewMatrix();
			LastTick = high_resolution_clock::now();
		}
	}
	GfxEngine.OnExit();
	return 0;
}
