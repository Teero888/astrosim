#include "graphics.h"
#include "../sim/body.h"
#include "../sim/starsystem.h"
#include "../sim/vmath.h"
#include "gfx/camera.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

bool CGraphics::OnInit(CStarSystem *pStarSystem)
{
	m_pStarSystem = pStarSystem;

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
	glfwSwapInterval(1);

	if(glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	printf("Renderer: %s\n", glGetString(GL_RENDERER));
	printf("OpenGL version supported %s\n", glGetString(GL_VERSION));

	glfwSetWindowUserPointer(m_pWindow, this);
	glfwSetCursorPosCallback(m_pWindow, MouseMotionCallback);
	glfwSetScrollCallback(m_pWindow, MouseScrollCallback);
	glfwSetKeyCallback(m_pWindow, KeyActionCallback);
	glfwSetWindowSizeCallback(m_pWindow, WindowSizeCallback);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	m_pImGuiIO = &ImGui::GetIO();
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(m_pWindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// Initialize Framebuffer
	int width, height;
	glfwGetWindowSize(m_pWindow, &width, &height);
	InitFramebuffer(width, height);

	m_Grid.Init();
	m_Atmosphere.Init();
	m_Trajectories.Init();
	m_Markers.Init();
	InitGfx();

	CleanupMeshes();
	OnBodiesReloaded(pStarSystem);

	return true;
}

void CGraphics::InitFramebuffer(int width, int height)
{
	if(m_GBufferFBO)
		glDeleteFramebuffers(1, &m_GBufferFBO);
	if(m_GBufferColorTex)
		glDeleteTextures(1, &m_GBufferColorTex);
	if(m_GBufferDepthTex)
		glDeleteTextures(1, &m_GBufferDepthTex);

	glGenFramebuffers(1, &m_GBufferFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_GBufferFBO);

	// Color Buffer
	glGenTextures(1, &m_GBufferColorTex);
	glBindTexture(GL_TEXTURE_2D, m_GBufferColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_GBufferColorTex, 0);

	// Depth Buffer
	glGenTextures(1, &m_GBufferDepthTex);
	glBindTexture(GL_TEXTURE_2D, m_GBufferDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_GBufferDepthTex, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CGraphics::ResizeFramebuffer(int width, int height)
{
	InitFramebuffer(width, height);
}

void CGraphics::CleanupMeshes()
{
	for(auto &[id, mesh] : m_BodyMeshes)
	{
		mesh->Destroy();
		delete mesh;
	}
	m_BodyMeshes.clear();
}

void CGraphics::OnBodiesReloaded(CStarSystem *pStarSystem)
{
	for(auto &Body : pStarSystem->m_vBodies)
	{
		m_BodyMeshes[Body.m_Id] = new CProceduralMesh();
		m_BodyMeshes[Body.m_Id]->Init(&Body, Body.m_RenderParams.m_BodyType, CProceduralMesh::VOXEL_RESOLUTION_DEFAULT);
	}

	m_Trajectories.ClearTrajectories();
}

void CGraphics::ReloadSimulation()
{
	printf("Hot-reloading simulation data...\n");

	std::string PrevFocusedBody = "";
	if(m_Camera.m_pFocusedBody)
		PrevFocusedBody = m_Camera.m_pFocusedBody->m_Name;

	CleanupMeshes();
	m_pStarSystem->OnInit();
	OnBodiesReloaded(m_pStarSystem);
	ResetCamera(m_pStarSystem, PrevFocusedBody);
}

void CGraphics::ResetCamera(CStarSystem *pStarSystem, std::string PrevFocusedBody)
{
	m_Camera.m_pFocusedBody = nullptr;
	if(!pStarSystem->m_vBodies.empty())
	{
		if(!PrevFocusedBody.empty())
		{
			for(auto &body : pStarSystem->m_vBodies)
			{
				if(body.m_Name == PrevFocusedBody)
				{
					m_Camera.SetBody(&body);
					break;
				}
			}
		}
		if(!m_Camera.m_pFocusedBody)
			m_Camera.SetBody(&pStarSystem->m_vBodies.front());
	}
	m_Camera.ResetCameraAngle();
}

void CGraphics::OnRender(CStarSystem &StarSystem)
{
	static auto s_LastFrame = std::chrono::steady_clock::now();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	float deltaTime = (float)m_FrameTime;
	if(!m_pImGuiIO->WantCaptureKeyboard)
	{
		if(glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(0, deltaTime);
		if(glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(1, deltaTime);
		if(glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(2, deltaTime);
		if(glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(3, deltaTime);
		if(glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(4, deltaTime);
		if(glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
			m_Camera.ProcessKeyboard(5, deltaTime);
	}

	if(std::fabs(m_Camera.m_WantedViewDistance - m_Camera.m_ViewDistance) > 1e-3)
		m_Camera.UpdateViewMatrix();

	// == UI Logic ==
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::BeginMenu("Windows"))
		{
			ImGui::MenuItem("Simulation Settings", NULL, &m_bShowSimSettings);
			ImGui::MenuItem("Camera Controls", NULL, &m_bShowCameraControls);
			ImGui::MenuItem("Planet Info", NULL, &m_bShowPlanetInfo);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if(m_bShowSimSettings)
	{
		ImGui::Begin("Simulation Settings", &m_bShowSimSettings);
		ImGui::Checkbox("Run Simulation", &m_bIsRunning);
		ImGui::SliderInt("Days per second", &StarSystem.m_DPS, 1, 365);
		ImGui::Separator();
		ImGui::Text("Rendering");
		ImGui::Checkbox("Show Atmosphere", &m_bShowAtmosphere);
		ImGui::Checkbox("Show Grid", &m_bShowGrid);
		ImGui::Checkbox("Show Trajectories", &m_Trajectories.m_ShowAll);
		ImGui::Checkbox("Show Markers", &m_Markers.m_ShowMarkers);
		ImGui::Checkbox("Wireframe Mode", &m_bShowWireframe);
		if(ImGui::Button("Reload Simulation (F5)"))
			m_bReloadRequested = true;
		ImGui::Text("FPS: %.1f", 1.0f / m_FrameTime);
		ImGui::End();
	}

	if(m_bShowCameraControls)
	{
		ImGui::Begin("Camera Controls", &m_bShowCameraControls);
		const char *modeName = (m_Camera.m_CameraMode == MODE_FOCUS) ? "Mode: Orbit (Focus)" : "Mode: Free Fly";
		if(ImGui::Button(modeName))
			m_Camera.ToggleMode();
		ImGui::SameLine();
		ImGui::TextDisabled("('C' to toggle)");
		ImGui::DragFloat("Speed Multiplier", &m_Camera.m_SpeedMultiplier, 0.1f, 0.1f, 100.0f);
		if(m_Camera.m_pFocusedBody)
		{
			double alt = m_Camera.m_ViewDistance - m_Camera.m_pFocusedBody->m_RenderParams.m_Radius;
			ImGui::Text("Altitude: %.2f km", alt / 1000.0);
			ImGui::Text("Rel Pos: %.2e, %.2e, %.2e", m_Camera.m_LocalPosition.x, m_Camera.m_LocalPosition.y, m_Camera.m_LocalPosition.z);
		}
		ImGui::End();
	}

	if(m_bShowPlanetInfo && m_Camera.m_pFocusedBody)
	{
		ImGui::Begin("Planet Info", &m_bShowPlanetInfo);
		const char *pCurrentItem = m_Camera.m_pFocusedBody->m_Name.c_str();
		if(ImGui::BeginCombo("Focus Body", pCurrentItem))
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
		auto &Body = *m_Camera.m_pFocusedBody;
		ImGui::Separator();
		ImGui::Text("Name: %s", Body.m_Name.c_str());
		ImGui::Text("Mass: %.4e kg", Body.m_SimParams.m_Mass);
		ImGui::Text("Radius: %.4e m", Body.m_RenderParams.m_Radius);
		ImGui::End();
	}

	// ========================================================
	// RENDER PASS 1: OPAQUE GEOMETRY TO FBO
	// ========================================================
	glBindFramebuffer(GL_FRAMEBUFFER, m_GBufferFBO);
	glViewport(0, 0, (int)m_Camera.m_ScreenSize.x, (int)m_Camera.m_ScreenSize.y);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	m_Trajectories.Render(m_Camera);

	if(m_bShowWireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	if(!StarSystem.m_vBodies.empty())
	{
		for(size_t i = 0; i < StarSystem.m_vBodies.size(); ++i)
		{
			auto &Body = StarSystem.m_vBodies[i];
			if(m_BodyMeshes.count(Body.m_Id))
			{
				auto mesh = m_BodyMeshes[Body.m_Id];
				mesh->Update(m_Camera);
				mesh->Render(m_Camera, m_pStarSystem->m_pSunBody);
			}
		}
	}
	if(m_bShowWireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// ========================================================
	// COPY FBO TO SCREEN
	// ========================================================
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBufferFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // Write to screen

	// Copy Color
	glBlitFramebuffer(0, 0, (int)m_Camera.m_ScreenSize.x, (int)m_Camera.m_ScreenSize.y,
		0, 0, (int)m_Camera.m_ScreenSize.x, (int)m_Camera.m_ScreenSize.y,
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	// Copy Depth (Essential for Markers/Grid to work correctly)
	glBlitFramebuffer(0, 0, (int)m_Camera.m_ScreenSize.x, (int)m_Camera.m_ScreenSize.y,
		0, 0, (int)m_Camera.m_ScreenSize.x, (int)m_Camera.m_ScreenSize.y,
		GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	// ========================================================
	// RENDER PASS 2: TRANSPARENTS (Screen)
	// ========================================================
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to screen

	// Render Atmosphere (reading from FBO Depth)
	if(m_bShowAtmosphere)
	{
		// Bind depth texture to Slot 0 for the shader to read
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_GBufferDepthTex);

		m_Atmosphere.Render(StarSystem, m_Camera, 0); // 0 is the texture unit
	}

	// Render Grid and Markers (Standard Depth Test against blitted depth)
	if(m_bShowGrid)
		m_Grid.Render(StarSystem, m_Camera);

	m_Markers.Render(StarSystem, m_Camera);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(m_pWindow);

	m_FrameTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - s_LastFrame).count() / 1e9;
	s_LastFrame = std::chrono::steady_clock::now();
}

void CGraphics::OnExit()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if(m_GBufferFBO)
		glDeleteFramebuffers(1, &m_GBufferFBO);
	if(m_GBufferColorTex)
		glDeleteTextures(1, &m_GBufferColorTex);
	if(m_GBufferDepthTex)
		glDeleteTextures(1, &m_GBufferDepthTex);

	CleanupMeshes();
	m_Atmosphere.Destroy();
	m_Grid.Destroy();
	m_Trajectories.Destroy();
	m_Markers.Destroy();
	glfwDestroyWindow(m_pWindow);
	glfwTerminate();
}

void CGraphics::MouseScrollCallback(GLFWwindow *pWindow, double XOffset, double YOffset)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics || pGraphics->m_pImGuiIO->WantCaptureMouse)
		return;

	if(pGraphics->m_Camera.m_pFocusedBody)
	{
		pGraphics->m_Camera.m_WantedViewDistance -= (pGraphics->m_Camera.m_WantedViewDistance / 10.f) * YOffset - (pGraphics->m_Camera.m_pFocusedBody->m_RenderParams.m_Radius / (10.f));
		pGraphics->m_Camera.UpdateViewMatrix();
	}
}

void CGraphics::MouseMotionCallback(GLFWwindow *pWindow, double XPos, double YPos)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
		return;

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
		return;

	if(Action == GLFW_PRESS || Action == GLFW_REPEAT)
	{
		if(Key == GLFW_KEY_W)
			MouseScrollCallback(pGraphics->m_pWindow, 0, 1);
		else if(Key == GLFW_KEY_S)
			MouseScrollCallback(pGraphics->m_pWindow, 0, -1);
		else if(Key == GLFW_KEY_LEFT) { /* ... Cycle bodies ... */ }
		else if(Key == GLFW_KEY_RIGHT) { /* ... Cycle bodies ... */ }
		else if(Key == GLFW_KEY_R && pGraphics->m_Camera.m_pFocusedBody)
			pGraphics->m_Camera.ResetCameraAngle();
		else if(Key == GLFW_KEY_F5)
			pGraphics->m_bReloadRequested = true;
		else if(Key == GLFW_KEY_C && Action == GLFW_PRESS)
			pGraphics->m_Camera.ToggleMode();
	}
}

void CGraphics::WindowSizeCallback(GLFWwindow *pWindow, int Width, int Height)
{
	CGraphics *pGraphics = ((CGraphics *)glfwGetWindowUserPointer(pWindow));
	if(!pGraphics)
		return;

	glViewport(0, 0, Width, Height);
	pGraphics->m_Camera.m_Projection = glm::perspective(glm::radians(pGraphics->m_Camera.m_FOV), (float)Width / (float)Height, NEAR_PLANE, FAR_PLANE);
	pGraphics->m_Camera.m_ScreenSize = glm::vec2(Width, Height);

	// Resize the Framebuffer textures
	pGraphics->ResizeFramebuffer(Width, Height);
}

void CGraphics::InitGfx() {}
