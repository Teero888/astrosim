#include "graphics.h"
#include "../sim/starsystem.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/geometric.hpp"
#include "shader.h"
#include <GLFW/glfw3.h>
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

static void GenerateSphere(float Radius, int Stacks, int Slices, std::vector<Vertex> &vVertices, std::vector<unsigned int> &vIndices)
{
	vVertices.clear();
	vIndices.clear();

	for(int i = 0; i <= Stacks; ++i)
	{
		float phi = static_cast<float>(i) / Stacks * glm::pi<float>();
		for(int j = 0; j <= Slices; ++j)
		{
			float theta = static_cast<float>(j) / Slices * 2.0f * glm::pi<float>();

			Vertex v;
			v.position.x = Radius * std::sin(phi) * std::cos(theta);
			v.position.y = Radius * std::cos(phi);
			v.position.z = Radius * std::sin(phi) * std::sin(theta);

			v.normal = glm::normalize(v.position);

			vVertices.push_back(v);
		}
	}

	for(int i = 0; i < Stacks; ++i)
	{
		for(int j = 0; j < Slices; ++j)
		{
			int first = (i * (Slices + 1)) + j;
			int second = first + Slices + 1;

			vIndices.push_back(first);
			vIndices.push_back(second);
			vIndices.push_back(first + 1);

			vIndices.push_back(second);
			vIndices.push_back(second + 1);
			vIndices.push_back(first + 1);
		}
	}
}

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

	// TODO: this is going to be moved to some function
	// Do sphere shader stuff
	{
		m_SphereShader.CompileShader(Shaders::VERT_SPHERE, Shaders::FRAG_SPHERE);
		// Generate sphere data
		std::vector<Vertex> vVertices;
		std::vector<unsigned int> vIndices;
		GenerateSphere(1.0f, 32, 32, vVertices, vIndices);
		m_SphereShader.m_NumIndices = vIndices.size();

		// Set up VBO, VAO, and EBO
		glGenVertexArrays(1, &m_SphereShader.VAO);
		glGenBuffers(1, &m_SphereShader.VBO);
		glGenBuffers(1, &m_SphereShader.EBO);

		glBindVertexArray(m_SphereShader.VAO);

		glBindBuffer(GL_ARRAY_BUFFER, m_SphereShader.VBO);
		glBufferData(GL_ARRAY_BUFFER, vVertices.size() * sizeof(Vertex), vVertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_SphereShader.EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, vIndices.size() * sizeof(unsigned int), vIndices.data(), GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, normal));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}
	// TODO: this shader is unused right now
	// Do solid shader stuff this is just for testing
	{
		m_SolidShader.CompileShader(Shaders::VERT_SOLID, Shaders::FRAG_SOLID);

		// Set up vertex data and buffers
		float vertices[] = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f};

		glGenVertexArrays(1, &m_SolidShader.VAO);
		glGenBuffers(1, &m_SolidShader.VBO);

		glBindVertexArray(m_SolidShader.VAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_SolidShader.VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	return true;
}

void CGraphics::OnRender()
{
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
	ImGui::SliderInt("Days per second", &m_pStarSystem->m_TPS, 1, 365);
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
	ImGui::Text("Name: %s", m_Camera.m_pFocusedBody->m_Name.c_str());
	ImGui::Text("Position: %.4e, %.4e, %.4e", m_Camera.m_pFocusedBody->m_Position.x, m_Camera.m_pFocusedBody->m_Position.y, m_Camera.m_pFocusedBody->m_Position.z);
	ImGui::Text("Velocity: %.4e, %.4e, %.4e", m_Camera.m_pFocusedBody->m_Velocity.x, m_Camera.m_pFocusedBody->m_Velocity.y, m_Camera.m_pFocusedBody->m_Velocity.z);
	ImGui::Text("Mass: %.4e", m_Camera.m_pFocusedBody->m_Mass);
	ImGui::Text("Radius: %.4e", m_Camera.m_pFocusedBody->m_Radius);
	ImGui::PopTextWrapPos();
	ImGui::End();

	// ImGui::ShowDemoWindow();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_Grid.Render(m_Camera);
	m_Trajectories.Render(m_Camera);

	// draw bodies
	m_pStarSystem->RenderBodies(&m_SphereShader, &m_Camera);
	// draw markers after bodies have been drawn
	m_Markers.Render(*m_pStarSystem, m_Camera);
	// draw test triangle xd
	// {
	// 	m_SolidShader.Use();
	// 	glBindVertexArray(m_SolidShader.VAO);
	// 	glDrawArrays(GL_TRIANGLES, 0, 3);
	// }

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(m_pWindow);
}

// cleanup
void CGraphics::OnExit()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	m_SolidShader.Destroy();
	m_SphereShader.Destroy();
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
