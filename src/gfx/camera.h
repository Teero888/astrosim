#ifndef CAMERA_H
#define CAMERA_H

#include "../sim/body.h"
#include "glm/ext/vector_float3.hpp"
#include <glm/gtc/type_ptr.hpp>

#define RENDER_SCALE 1e8

enum ECameraMode
{
	MODE_FOCUS = 0,
	MODE_FREEVIEW,
};

// Camera nightmare fuel
struct CCamera
{
	SBody *m_pFocusedBody = nullptr;

	float m_Radius = 50.f;
	Vec3 m_FocusPoint = Vec3(0, 0, 0);
	ECameraMode m_CameraMode = MODE_FOCUS;
	glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 10.0f);
	// Camera Directions
	glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 m_Right = glm::normalize(glm::cross(m_Front, m_Up));

	float m_Yaw = -90.0f; // Yaw angle (left/right rotation)
	float m_Pitch = 0.0f; // Pitch angle (up/down rotation)
	float m_Speed = 2.5f; // Movement speed
	float m_Sensitivity = 0.1f; // Mouse sensitivity

	glm::mat4 m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
	glm::mat4 m_Projection = glm::perspective(glm::radians(70.0f), 1600.0f / 1000.0f, 0.1f, 1000000.0f);

	void UpdateViewMatrix();
	void ProcessKeyboard(int direction, float deltaTime);
	void ProcessMouse(float xoffset, float yoffset);
};

#endif // CAMERA_H
