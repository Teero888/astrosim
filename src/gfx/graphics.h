#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "atmosphere.h"
#include "camera.h"
#include "grid.h"
#include "imgui.h"
#include "markers.h"
#include "proceduralmesh.h"
#include "trajectories.h"
#include <map>

class CStarSystem;
struct SBody;
class GLFWwindow;

class CGraphics
{
	GLFWwindow *m_pWindow = nullptr;
	CStarSystem *m_pStarSystem = nullptr;

	CGrid m_Grid;
	CAtmosphere m_Atmosphere;
	std::map<int, CProceduralMesh *> m_BodyMeshes;

	// == NEW: Framebuffer for Depth Reading ==
	GLuint m_GBufferFBO = 0;
	GLuint m_GBufferColorTex = 0;
	GLuint m_GBufferDepthTex = 0;
	void InitFramebuffer(int width, int height);
	void ResizeFramebuffer(int width, int height);

	static void MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset);
	static void MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos);
	static void MouseButtonCallback(GLFWwindow *pWindow, int Button, int Action, int Mods);
	static void KeyActionCallback(GLFWwindow *pWindow, int Key, int Scancode, int Action, int Mods);
	static void WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height);

public:
	float m_FrameTime = 0.0f;
	bool m_bShowWireframe = false;
	bool m_bShowAtmosphere = true;
	bool m_bShowGrid = true; // NEW: Grid Toggle
	bool m_bReloadRequested = false;
	bool m_bIsRunning = true;

	// UI Toggles
	bool m_bShowSimSettings = true;
	bool m_bShowCameraControls = true;
	bool m_bShowPlanetInfo = true;

	ImGuiIO *m_pImGuiIO;
	CCamera m_Camera;
	CTrajectories m_Trajectories;
	CMarkers m_Markers;
	bool OnInit(CStarSystem *pStarSystem);
	void InitGfx();
	void OnRender(CStarSystem &StarSystem);
	void OnExit();
	void ReloadSimulation();
	void OnBodiesReloaded(CStarSystem *pStarSystem);
	void CleanupMeshes();

	inline GLFWwindow *GetWindow() { return m_pWindow; }

private:
	void ResetCamera(CStarSystem *pStarSystem, std::string PrevFocusedBody = "");
};

#endif // GRAPHICS_H
