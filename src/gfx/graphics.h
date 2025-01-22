#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "camera.h"
#include "grid.h"
#include "imgui.h"
#include "shader.h"

class CStarSystem;
class GLFWwindow;

class CGraphics
{
	GLFWwindow *m_pWindow = nullptr;
	CStarSystem *m_pStarSystem = nullptr;

	CShader m_SolidShader;
	CShader m_SphereShader;

	// beautiful nice grid by meine wenigkeit
	CGrid m_Grid;

	static void MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset);
	static void MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos);
	static void MouseButtonCallback(GLFWwindow *pWindow, int Button, int Action, int Mods);
	static void KeyActionCallback(GLFWwindow *pWindow, int Key, int Scancode, int Action, int Mods);
	static void WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height);

public:
	// needs to be public for controls stuff since theyre static
	ImGuiIO *m_pImGuiIO;
	CCamera m_Camera;
	bool OnInit(CStarSystem *pStarSystem);
	void OnRender();
	void OnExit();

	inline GLFWwindow *GetWindow() { return m_pWindow; }
};

#endif // GRAPHICS_H
