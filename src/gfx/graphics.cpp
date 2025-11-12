#include "graphics.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "shader.h"
#include "generated/embedded_shaders.h"
#include "../sim/body.h"
#include "../sim/vmath.h"
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
#include <map>

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
	InitGfx();

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

	ImGui::Checkbox("Show all trajectories", &m_Trajectories.m_ShowAll);
	ImGui::Checkbox("Show markers", &m_Markers.m_ShowMarkers);
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
	RenderBodies(StarSystem.m_vBodies, m_Camera);
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
	pGraphics->m_Camera.m_WantedViewDistance -= (pGraphics->m_Camera.m_WantedViewDistance / 10.f) * YOffset;
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
			pGraphics->m_Camera.m_WantedViewDistance -= pGraphics->m_Camera.m_WantedViewDistance / 10.f;
			pGraphics->m_Camera.UpdateViewMatrix();
		}
		else if(Key == GLFW_KEY_S)
		{
			pGraphics->m_Camera.m_WantedViewDistance += pGraphics->m_Camera.m_WantedViewDistance / 10.f;
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

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

// Helper function to get a midpoint vertex, creating it if it doesn't exist
static unsigned int GetMidpoint(unsigned int p1, unsigned int p2, std::vector<Vertex>& vertices, std::map<long long, unsigned int>& MidpointCache, float Radius)
{
    bool FirstIsSmaller = p1 < p2;
    long long SmallerIndex = FirstIsSmaller ? p1 : p2;
    long long GreaterIndex = FirstIsSmaller ? p2 : p1;
    long long Key = (SmallerIndex << 32) + GreaterIndex;

    if (MidpointCache.count(Key))
    {
        return MidpointCache[Key];
    }

    Vertex v1 = vertices[p1];
    Vertex v2 = vertices[p2];
    glm::vec3 Mid = glm::normalize(v1.position + v2.position) * Radius;

    Vertex NewV;
    NewV.position = Mid;
    NewV.normal = glm::normalize(Mid); // Normal is just the normalized position for a sphere

    vertices.push_back(NewV);
    unsigned int Index = vertices.size() - 1;
    MidpointCache[Key] = Index;
    return Index;
}

static void GenerateSphere(float Radius, int Subdivisions, std::vector<Vertex>& vVertices, std::vector<unsigned int>& vIndices)
{
    vVertices.clear();
    vIndices.clear();
    std::map<long long, unsigned int> mMidpointCache;

    // Create 12 vertices of an icosahedron
	#define SQRT_5__ 2.236067977499789696409173f
    float t = (1.0f + SQRT_5__) / 2.0f;
	#undef SQRT_5__

    vVertices.push_back({{glm::normalize(glm::vec3(-1, t, 0)) * Radius}, {glm::normalize(glm::vec3(-1, t, 0))}});
    vVertices.push_back({{glm::normalize(glm::vec3(1, t, 0)) * Radius}, {glm::normalize(glm::vec3(1, t, 0))}});
    vVertices.push_back({{glm::normalize(glm::vec3(-1, -t, 0)) * Radius}, {glm::normalize(glm::vec3(-1, -t, 0))}});
    vVertices.push_back({{glm::normalize(glm::vec3(1, -t, 0)) * Radius}, {glm::normalize(glm::vec3(1, -t, 0))}});

    vVertices.push_back({{glm::normalize(glm::vec3(0, -1, t)) * Radius}, {glm::normalize(glm::vec3(0, -1, t))}});
    vVertices.push_back({{glm::normalize(glm::vec3(0, 1, t)) * Radius}, {glm::normalize(glm::vec3(0, 1, t))}});
    vVertices.push_back({{glm::normalize(glm::vec3(0, -1, -t)) * Radius}, {glm::normalize(glm::vec3(0, -1, -t))}});
    vVertices.push_back({{glm::normalize(glm::vec3(0, 1, -t)) * Radius}, {glm::normalize(glm::vec3(0, 1, -t))}});

    vVertices.push_back({{glm::normalize(glm::vec3(t, 0, -1)) * Radius}, {glm::normalize(glm::vec3(t, 0, -1))}});
    vVertices.push_back({{glm::normalize(glm::vec3(t, 0, 1)) * Radius}, {glm::normalize(glm::vec3(t, 0, 1))}});
    vVertices.push_back({{glm::normalize(glm::vec3(-t, 0, -1)) * Radius}, {glm::normalize(glm::vec3(-t, 0, -1))}});
    vVertices.push_back({{glm::normalize(glm::vec3(-t, 0, 1)) * Radius}, {glm::normalize(glm::vec3(-t, 0, 1))}});

    // 5 faces around point 0
    vIndices.push_back(0); vIndices.push_back(11); vIndices.push_back(5);
    vIndices.push_back(0); vIndices.push_back(5); vIndices.push_back(1);
    vIndices.push_back(0); vIndices.push_back(1); vIndices.push_back(7);
    vIndices.push_back(0); vIndices.push_back(7); vIndices.push_back(10);
    vIndices.push_back(0); vIndices.push_back(10); vIndices.push_back(11);

    // 5 adjacent faces
    vIndices.push_back(1); vIndices.push_back(5); vIndices.push_back(9);
    vIndices.push_back(5); vIndices.push_back(11); vIndices.push_back(4);
    vIndices.push_back(11); vIndices.push_back(10); vIndices.push_back(2);
    vIndices.push_back(10); vIndices.push_back(7); vIndices.push_back(6);
    vIndices.push_back(7); vIndices.push_back(1); vIndices.push_back(8);

    // 5 faces around point 3
    vIndices.push_back(3); vIndices.push_back(9); vIndices.push_back(4);
    vIndices.push_back(3); vIndices.push_back(4); vIndices.push_back(2);
    vIndices.push_back(3); vIndices.push_back(2); vIndices.push_back(6);
    vIndices.push_back(3); vIndices.push_back(6); vIndices.push_back(8);
    vIndices.push_back(3); vIndices.push_back(8); vIndices.push_back(9);

    // 5 adjacent faces
    vIndices.push_back(4); vIndices.push_back(9); vIndices.push_back(5);
    vIndices.push_back(2); vIndices.push_back(4); vIndices.push_back(11);
    vIndices.push_back(6); vIndices.push_back(2); vIndices.push_back(10);
    vIndices.push_back(8); vIndices.push_back(6); vIndices.push_back(7);
    vIndices.push_back(9); vIndices.push_back(8); vIndices.push_back(1);

    // Subdivide
    for (int i = 0; i < Subdivisions; ++i)
    {
        std::vector<unsigned int> vNewIndices;
        for (size_t j = 0; j < vIndices.size(); j += 3)
        {
            unsigned int v1 = vIndices[j];
            unsigned int v2 = vIndices[j + 1];
            unsigned int v3 = vIndices[j + 2];

            unsigned int a = GetMidpoint(v1, v2, vVertices, mMidpointCache, Radius);
            unsigned int b = GetMidpoint(v2, v3, vVertices, mMidpointCache, Radius);
            unsigned int c = GetMidpoint(v3, v1, vVertices, mMidpointCache, Radius);

            vNewIndices.push_back(v1); vNewIndices.push_back(a); vNewIndices.push_back(c);
            vNewIndices.push_back(v2); vNewIndices.push_back(b); vNewIndices.push_back(a);
            vNewIndices.push_back(v3); vNewIndices.push_back(c); vNewIndices.push_back(b);
            vNewIndices.push_back(a); vNewIndices.push_back(b); vNewIndices.push_back(c);
        }
        vIndices = vNewIndices;
    }
}

void CGraphics::InitGfx()
{
	m_BodyShader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	std::vector<Vertex> vVertices;
	std::vector<unsigned int> vIndices;
	GenerateSphere(1.0f, 3, vVertices, vIndices);
	m_BodyShader.m_NumIndices = vIndices.size();

	// Set up VBO, VAO, and EBO
	glGenVertexArrays(1, &m_BodyShader.VAO);
	glGenBuffers(1, &m_BodyShader.VBO);
	glGenBuffers(1, &m_BodyShader.EBO);

	glBindVertexArray(m_BodyShader.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_BodyShader.VBO);
	glBufferData(GL_ARRAY_BUFFER, vVertices.size() * sizeof(Vertex), vVertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_BodyShader.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, vIndices.size() * sizeof(unsigned int), vIndices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);
}

glm::vec3 CGraphics::WorldToClip(const Vec3 &WorldPos, const CCamera &Camera)
{
    Vec3 NewPos = (WorldPos - Camera.m_pFocusedBody->m_SimParams.m_Position) / Camera.m_ViewDistance;
    return (glm::vec3)NewPos;
}

void CGraphics::RenderBody(const SBody *pBody, const SBody *pLightBody, CCamera &Camera)
{
	// set transformation matrices
	glm::mat4 Model = glm::mat4(1.0f);
	Model = glm::translate(Model, WorldToClip(pBody->m_SimParams.m_Position, Camera));

	m_BodyShader.SetBool("uSource", pBody == pLightBody);
	m_BodyShader.SetFloat("uRadius", pBody->m_RenderParams.m_Radius / Camera.m_ViewDistance);
	m_BodyShader.SetMat4("uModel", Model);
	m_BodyShader.SetMat4("uView", Camera.m_View);
	m_BodyShader.SetMat4("uProjection", Camera.m_Projection);

	// set light properties
	Vec3 NewLightDir = (pLightBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position).normalize();
	m_BodyShader.SetVec3("uLightDir", (glm::vec3)NewLightDir);
	m_BodyShader.SetVec3("uLightColor", pLightBody->m_RenderParams.m_Color);
	m_BodyShader.SetVec3("uObjectColor", pBody->m_RenderParams.m_Color);

	// bind the vao and draw the sphere
	glBindVertexArray(m_BodyShader.VAO);
	glDrawElements(GL_TRIANGLES, m_BodyShader.m_NumIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void CGraphics::RenderBodies(const std::vector<SBody> &vBodies, CCamera &Camera)
{
	if (vBodies.empty())
		return;

	m_BodyShader.Use();
	for(const auto &Body : vBodies)
		RenderBody(&Body, &vBodies.front(), Camera);
}

