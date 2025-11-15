#include "graphics.h"
#include "../sim/body.h"
#include "../sim/starsystem.h"
#include "../sim/vmath.h"
#include "camera.h"
#include "shader.h"
#include <FastNoiseLite.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <map>
#include <vector>

bool CGraphics::OnInit(CStarSystem *pStarSystem)
{
	m_pStarSystem = pStarSystem;

	// init glfw
	if(!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW" << std::endl;
		return false;
	}

	m_pWindow = glfwCreateWindow(1600, 1000, "AstroSim", nullptr, nullptr);
	if(!m_pWindow)
	{
		glfwTerminate();
		std::cerr << "Failed to create GLFW window" << std::endl;
		return false;
	}

	glfwMakeContextCurrent(m_pWindow);
	glfwSwapInterval(1); // enable vsync

	// init glew
	if(glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	// noice print
	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL version supported %s\n", glGetString(GL_VERSION));

	// Setup callbacks for inputs (i hate it xd)
	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetCursorPosCallback(m_pWindow, MouseMotionCallback);
	glfwSetScrollCallback(m_pWindow, MouseScrollCallback);
	glfwSetKeyCallback(m_pWindow, KeyActionCallback);
	glfwSetWindowSizeCallback(m_pWindow, WindowSizeCallback);

	// Setup ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_pImGuiIO = &ImGui::GetIO();
	ImGui::StyleColorsDark();

	// setup renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(m_pWindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	glEnable(GL_DEPTH_TEST); // this is so things have the right order
	glDepthMask(GL_TRUE); // enable depth writing

	m_Grid.Init();
	m_Trajectories.Init();
	m_Markers.Init();
	InitGfx(); // This is now empty, but we call it for consistency

	// Initialize procedural meshes for ALL bodies
	for(auto &Body : pStarSystem->m_vBodies)
	{
		m_BodyMeshes[Body.m_Id] = new CProceduralMesh();
		m_BodyMeshes[Body.m_Id]->Init(&Body);
	}

	return true;
}

void CGraphics::OnRender(CStarSystem &StarSystem)
{
	static auto s_LastFrame = std::chrono::steady_clock::now();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if(std::fabs(m_Camera.m_WantedViewDistance - m_Camera.m_ViewDistance) > 1e-3)
		m_Camera.UpdateViewMatrix();

	// use imgui later xd
	// ImGui::Begin("Style Editor");
	// ImGui::ShowStyleEditor();
	// ImGui::End();
	//
	// // Render a simple ImGui window

	ImGui::Begin("Settings");
	ImGui::SliderInt("Days per second", &StarSystem.m_DPS, 1, 365);
	static const char *pCurrentItem = StarSystem.m_vBodies.front().m_Name.c_str();
	pCurrentItem = m_Camera.m_pFocusedBody->m_Name.c_str();

	ImGui::SliderInt("LOD", &m_Camera.m_LOD, 0, COctreeNode::MAX_LOD_LEVEL);
	ImGui::Checkbox("Show all trajectories", &m_Trajectories.m_ShowAll);
	ImGui::Checkbox("Show markers", &m_Markers.m_ShowMarkers);
	ImGui::Checkbox("Show Wireframe", &m_bShowWireframe);
	if(ImGui::BeginCombo("Select Focus##focus", pCurrentItem))
	{
		for(auto &Body : StarSystem.m_vBodies)
		{
			bool IsSelected = (pCurrentItem == Body.m_Name.c_str());
			if(ImGui::Selectable(Body.m_Name.c_str(), IsSelected))
				m_Camera.SetBody(&Body);
			if(IsSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::PushTextWrapPos(5000.f);
	auto &Body = *m_Camera.m_pFocusedBody;
	ImGui::Text("Name: %s", Body.m_Name.c_str());
	ImGui::Text("Position: %.4e, %.4e, %.4e", Body.m_SimParams.m_Position.x, Body.m_SimParams.m_Position.y, Body.m_SimParams.m_Position.z);
	ImGui::Text("Velocity: %.4e, %.4e, %.4e", Body.m_SimParams.m_Velocity.x, Body.m_SimParams.m_Velocity.y, Body.m_SimParams.m_Velocity.z);
	ImGui::Text("Mass: %.4e", Body.m_SimParams.m_Mass);
	ImGui::Text("Radius: %.4e", Body.m_RenderParams.m_Radius);
	ImGui::PopTextWrapPos();
	ImGui::Text("FPS: %.2f", 1.f / m_FrameTime);
	ImGui::End();

	// ImGui::ShowDemoWindow();

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthFunc(GL_LESS);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_Trajectories.Render(m_Camera);
	if(m_bShowWireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	m_Camera.m_LowestDist = DBL_MAX;
	m_Camera.m_HighestDist = -DBL_MAX;

	for(size_t i = 0; i < StarSystem.m_vBodies.size(); ++i)
	{
		auto &Body = StarSystem.m_vBodies[i];
		if(m_BodyMeshes.count(Body.m_Id))
		{
			auto mesh = m_BodyMeshes[Body.m_Id];
			mesh->Update(m_Camera); // Update LOD
			// Render, using the Sun (body 0) as the light source
			mesh->Render(m_Camera, &StarSystem.m_vBodies.front());
		}
	}
	// printf("min: %.7f, max: %.4f\n", m_Camera.m_LowestDist, m_Camera.m_HighestDist);

	if(m_bShowWireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	m_Grid.Render(StarSystem, m_Camera);
	// draw markers after bodies have been drawn
	m_Markers.Render(StarSystem, m_Camera);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(m_pWindow);

	m_FrameTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - s_LastFrame).count() / 1e9;
	s_LastFrame = std::chrono::steady_clock::now();
}

// cleanup
void CGraphics::OnExit()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	// Clean up procedural meshes
	for(auto &[id, mesh] : m_BodyMeshes)
	{
		mesh->Destroy();
		delete mesh;
	}
	m_BodyMeshes.clear();

	m_Grid.Destroy();
	m_Trajectories.Destroy();
	m_Markers.Destroy();
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

void CGraphics::MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
	{
		printf("Window pointer does not exist in scroll callback. very very bad\n");
		return;
	}
	if(pGraphics->m_pImGuiIO->WantCaptureMouse)
		return;
	pGraphics->m_Camera.m_WantedViewDistance -= (pGraphics->m_Camera.m_WantedViewDistance / 10.f) * YOffset - pGraphics->m_Camera.m_pFocusedBody->m_RenderParams.m_Radius / 1000.f;
	pGraphics->m_Camera.UpdateViewMatrix();
}

void CGraphics::MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
	{
		printf("Window pointer does not exist in motion callback. very very bad\n");
		return;
	}
	static double LastX = XPos, LastY = YPos;

	if(glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_1) && !pGraphics->m_pImGuiIO->WantCaptureMouse)
	{
		pGraphics->m_Camera.ProcessMouse(XPos - LastX, LastY - YPos);
		pGraphics->m_Camera.UpdateViewMatrix();
	}
	LastX = XPos, LastY = YPos;
}

void CGraphics::MouseButtonCallback(GLFWwindow *pWindow, int Button, int Action, int Mods) {}
void CGraphics::KeyActionCallback(GLFWwindow *pWindow, int Key, int Scancode, int Action, int Mods)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
	{
		printf("Window pointer does not exist in scroll callback. very very bad\n");
		return;
	}
	if(Action > 0)
	{
		if(Key == GLFW_KEY_W)
		{
			MouseScrollCallback(pGraphics->m_pWindow, 0, 1);
			pGraphics->m_Camera.UpdateViewMatrix();
		}
		else if(Key == GLFW_KEY_S)
		{
			MouseScrollCallback(pGraphics->m_pWindow, 0, -1);
			pGraphics->m_Camera.UpdateViewMatrix();
		}
		else if(Key == GLFW_KEY_LEFT)
		{
			pGraphics->m_Camera.SetBody(&pGraphics->m_pStarSystem->m_vBodies[(pGraphics->m_Camera.m_pFocusedBody->m_Id - 1) % pGraphics->m_pStarSystem->m_vBodies.size()]);
		}
		else if(Key == GLFW_KEY_RIGHT)
		{
			pGraphics->m_Camera.SetBody(&pGraphics->m_pStarSystem->m_vBodies[(pGraphics->m_Camera.m_pFocusedBody->m_Id + 1) % pGraphics->m_pStarSystem->m_vBodies.size()]);
		}
	}
}

void CGraphics::WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
	{
		printf("Window pointer does not exist in window resize callback. very very bad\n");
		return;
	}
	glViewport(0, 0, Width, Height);
	pGraphics->m_Camera.m_Projection = glm::perspective(glm::radians(pGraphics->m_Camera.m_FOV), (float)Width / (float)Height, 0.1f, 1e9f);
	pGraphics->m_Camera.m_ScreenSize = glm::vec2(Width, Height);
}

void CGraphics::InitGfx()
{
}
