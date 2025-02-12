#include "graphics.h"
#include "../sim/starsystem.h"
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
#include <vector>

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

// static void GenerateSphere(float Radius, int Stacks, int Slices, std::vector<Vertex> &vVertices, std::vector<unsigned int> &vIndices)
// {
// 	FastNoiseLite Noise;
// 	Noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
// 	Noise.SetFrequency(1.f);
//
// 	vVertices.clear();
// 	vIndices.clear();
//
// 	for(int i = 0; i <= Stacks; ++i)
// 	{
// 		float phi = static_cast<float>(i) / Stacks * glm::pi<float>();
// 		for(int j = 0; j <= Slices; ++j)
// 		{
// 			float theta = static_cast<float>(j) / Slices * 2.0f * glm::pi<float>();
//
// 			Vertex v;
// 			v.position.x = Radius * std::sin(phi) * std::cos(theta);
// 			v.position.y = Radius * std::cos(phi);
// 			v.position.z = Radius * std::sin(phi) * std::sin(theta);
//
// 			// float Mod = (Noise.GetNoise(v.position.x, v.position.y, v.position.z) + 1.f) / 4.f;
// 			// v.position.x *= Mod;
// 			// v.position.y *= Mod;
// 			// v.position.z *= Mod;
//
// 			v.normal = glm::normalize(v.position);
//
// 			vVertices.push_back(v);
// 		}
// 	}
//
// 	for(int i = 0; i < Stacks; ++i)
// 	{
// 		for(int j = 0; j < Slices; ++j)
// 		{
// 			int first = (i * (Slices + 1)) + j;
// 			int second = first + Slices + 1;
//
// 			vIndices.push_back(first);
// 			vIndices.push_back(second);
// 			vIndices.push_back(first + 1);
//
// 			vIndices.push_back(second);
// 			vIndices.push_back(second + 1);
// 			vIndices.push_back(first + 1);
// 		}
// 	}
// }

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
	m_pStarSystem->InitGfx();

	return true;
}

void CGraphics::OnRender()
{
	static auto s_LastFrame = std::chrono::steady_clock::now();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if(std::fabs(m_Camera.m_WantedRadius - m_Camera.m_Radius) > 1e-3)
		m_Camera.UpdateViewMatrix();

	// use imgui later xd
	// ImGui::Begin("Style Editor");
	// ImGui::ShowStyleEditor();
	// ImGui::End();
	//
	// // Render a simple ImGui window

	ImGui::Begin("Settings");
	ImGui::SliderInt("Days per second", &m_pStarSystem->m_DPS, 1, 365);
	static const char *pCurrentItem = m_pStarSystem->m_vBodies.front().m_Name.c_str();
	pCurrentItem = m_Camera.m_pFocusedBody->m_Name.c_str();

	ImGui::Checkbox("Show all trajectories", &m_Trajectories.m_ShowAll);
	if(ImGui::BeginCombo("Select Focus##focus", pCurrentItem))
	{
		for(auto &Body : m_pStarSystem->m_vBodies)
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

	// auto &TP = Body.m_RenderParams.m_TerrainParams;
	//
	// // Continent parameters
	// ImGui::SliderFloat("Continent Base Height", &TP.continentBaseHeight, 0.0f, 2.0f, "%.2f");
	// ImGui::SliderFloat("Continent Frequency", &TP.continentFrequency, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Continent Amplitude", &TP.continentAmplitude, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Continent Lacunarity", &TP.continentLacunarity, 1.0f, 4.0f, "%.2f");
	// ImGui::SliderFloat("Continent Gain", &TP.continentGain, 0.0f, 1.0f, "%.2f");
	// ImGui::SliderInt("Continent Octaves", &TP.continentOctaves, 1, 10);
	//
	// // Mountain parameters
	// ImGui::SliderFloat("Mountain Amplitude", &TP.mountainAmplitude, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Mountain Frequency", &TP.mountainFrequency, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Mountain Vertical Sharpness", &TP.mountainVerticalSharpness, 0.0f, 5.0f, "%.2f");
	// ImGui::SliderFloat("Mountain Warping Strength", &TP.mountainWarpingStrength, 0.0f, 10.0f, "%.2f");
	//
	// // Ridge parameters
	// ImGui::SliderFloat("Ridge Scale", &TP.ridgeScale, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Ridge Offset", &TP.ridgeOffset, -5.0f, 5.0f, "%.2f");
	// ImGui::SliderFloat("Ridge Amplitude", &TP.ridgeAmplitude, 0.0f, 10.0f, "%.2f");
	//
	// // Canyon parameters
	// ImGui::SliderFloat("Canyon Depth", &TP.canyonDepth, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Canyon Width", &TP.canyonWidth, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Canyon Frequency", &TP.canyonFrequency, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Canyon Edge Sharpness", &TP.canyonEdgeSharpness, 0.0f, 10.0f, "%.2f");
	//
	// // Crater parameters
	// ImGui::SliderFloat("Crater Density", &TP.craterDensity, 0.0f, 1.0f, "%.2f");
	// ImGui::SliderFloat("Crater Depth", &TP.craterDepth, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Crater Radius Variation", &TP.craterRadiusVariation, 0.0f, 1.0f, "%.2f");
	// ImGui::SliderFloat("Crater Smoothness", &TP.craterSmoothness, 0.0f, 10.0f, "%.2f");
	//
	// // Noise parameters
	// ImGui::SliderFloat("Noise Warping Strength", &TP.noiseWarpingStrength, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Detail Scale", &TP.detailScale, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Detail Amplitude", &TP.detailAmplitude, 0.0f, 10.0f, "%.2f");
	//
	// // Erosion parameters
	// ImGui::SliderFloat("Erosion Scale", &TP.erosionScale, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Erosion Intensity", &TP.erosionIntensity, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Sediment Deposit", &TP.sedimentDeposit, 0.0f, 10.0f, "%.2f");
	//
	// // Polar parameters
	// ImGui::SliderFloat("Polar Cap Height", &TP.polarCapHeight, 0.0f, 2.0f, "%.2f");
	// ImGui::SliderFloat("Polar Cap Sharpness", &TP.polarCapSharpness, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Polar Latitude", &TP.polarLatitude, 0.0f, 90.0f, "%.2f");
	//
	// // Advanced parameters
	// ImGui::SliderFloat("Tectonic Plates", &TP.tectonicPlates, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Volcanic Activity", &TP.volcanicActivity, 0.0f, 10.0f, "%.2f");
	// ImGui::SliderFloat("Sedimentary Layering", &TP.sedimentaryLayering, 0.0f, 10.0f, "%.2f");
	//
	// // Blend controls
	// ImGui::SliderFloat("Mountain-Continent Blend", &TP.mountainContinentBlend, 0.0f, 1.0f, "%.2f");
	// ImGui::SliderFloat("Canyon-Mountain Blend", &TP.canyonMountainBlend, 0.0f, 1.0f, "%.2f");
	// ImGui::SliderFloat("Crater Blend Sharpness", &TP.craterBlendSharpness, 0.0f, 10.0f, "%.2f");
	//
	// // Toggles
	// ImGui::Checkbox("Enable Craters", &TP.enableCraters);
	// ImGui::Checkbox("Enable Canyons", &TP.enableCanyons);
	// ImGui::Checkbox("Enable Polar Caps", &TP.enablePolarCaps);

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

	m_Grid.Render(*m_pStarSystem, m_Camera);
	m_Trajectories.Render(m_Camera);
	// draw bodies
	m_pStarSystem->RenderBodies(m_Camera);
	// draw markers after bodies have been drawn
	m_Markers.Render(*m_pStarSystem, m_Camera);

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
	pGraphics->m_Camera.m_WantedRadius -= (pGraphics->m_Camera.m_WantedRadius / 10.f) * YOffset;
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
			pGraphics->m_Camera.m_WantedRadius -= pGraphics->m_Camera.m_WantedRadius / 10.f;
			pGraphics->m_Camera.UpdateViewMatrix();
		}
		else if(Key == GLFW_KEY_S)
		{
			pGraphics->m_Camera.m_WantedRadius += pGraphics->m_Camera.m_WantedRadius / 10.f;
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
	pGraphics->m_Camera.m_Projection = glm::perspective(glm::radians(70.0f), (float)Width / (float)Height, 0.1f, 1e9f);
	pGraphics->m_Camera.m_ScreenSize = glm::vec2(Width, Height);
}
