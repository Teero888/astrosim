#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "camera.h"
#include "grid.h"
#include "imgui.h"
#include "markers.h"
#include "shader.h"
#include "trajectories.h"
#include <vector>

#define DEFAULT_SCALE 100.0

class CStarSystem;
struct SBody;
class GLFWwindow;

class CGraphics
{
	GLFWwindow *m_pWindow = nullptr;
	CStarSystem *m_pStarSystem = nullptr;

	// beautiful nice grid by meine wenigkeit
	CGrid m_Grid;

	CShader m_BodyShader;
	void RenderBody(const SBody *pBody, const SBody *pLightBody, CCamera &Camera);
	void RenderBodies(const std::vector<SBody> &vBodies, CCamera &Camera);
	glm::vec3 WorldToClip(const Vec3 &WorldPos, const CCamera &Camera);

	static void MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset);
	static void MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos);
	static void MouseButtonCallback(GLFWwindow *pWindow, int Button, int Action, int Mods);
	static void KeyActionCallback(GLFWwindow *pWindow, int Key, int Scancode, int Action, int Mods);
	static void WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height);

public:
	float m_FrameTime = 0.0f;
	// needs to be public for controls stuff since theyre static
	ImGuiIO *m_pImGuiIO;
	CCamera m_Camera;
	CTrajectories m_Trajectories;
	CMarkers m_Markers;
	bool OnInit(CStarSystem *pStarSystem);
	void InitGfx();
	void OnRender(CStarSystem &StarSystem);
	void OnExit();

	inline GLFWwindow *GetWindow() { return m_pWindow; }
};

#endif // GRAPHICS_H
