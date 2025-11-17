#ifndef GRAPHICS_H
#define GRAPHICS_H

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

	// beautiful nice grid by meine wenigkeit
	CGrid m_Grid;
	std::map<int, CProceduralMesh *> m_BodyMeshes;

	static void MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset);
	static void MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos);
	static void MouseButtonCallback(GLFWwindow *pWindow, int Button, int Action, int Mods);
	static void KeyActionCallback(GLFWwindow *pWindow, int Key, int Scancode, int Action, int Mods);
	static void WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height);

public:
	float m_FrameTime = 0.0f;
	bool m_bShowWireframe = false;
	// needs to be public for controls stuff since they're static
	ImGuiIO *m_pImGuiIO;
	CCamera m_Camera;
	CTrajectories m_Trajectories;
	CMarkers m_Markers;
	bool OnInit(CStarSystem *pStarSystem);
	void InitGfx(); // This is now an empty function, but kept for structure
	void OnRender(CStarSystem &StarSystem);
	void OnExit();
	void ReloadSimulation();
	void OnBodiesReloaded(CStarSystem *pStarSystem);

	inline GLFWwindow *GetWindow() { return m_pWindow; }

private:
	void ResetCamera(CStarSystem *pStarSystem);
};

#endif // GRAPHICS_H
