#ifndef CAMERA_H
#define CAMERA_H

#include "../sim/body.h"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/gtc/type_ptr.hpp>

#define ZOOM_FACTOR 10.0

enum ECameraMode
{
	MODE_FOCUS = 0,
	MODE_FREEVIEW,
};

// Camera nightmare fuel
struct CCamera
{
	SBody *m_pFocusedBody = nullptr;

	double m_Radius = 1e8;
	double m_WantedRadius = 1e8;

	glm::vec2 m_ScreenSize; // get only
	Vec3 m_FocusPoint = Vec3(0, 0, 0);
	ECameraMode m_CameraMode = MODE_FOCUS;
	glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 10.0f);
	glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 m_Right = glm::normalize(glm::cross(m_Front, m_Up));

	float m_Yaw = 45.0f; // Yaw angle (left/right rotation)
	float m_Pitch = 30.0f; // Pitch angle (up/down rotation)
	float m_Speed = 2.5f; // Movement speed
	float m_Sensitivity = 0.1f; // Mouse sensitivity

	float m_FOV = 70.f;
	glm::mat4 m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
	glm::mat4 m_Projection = glm::perspective(glm::radians(m_FOV), 1600.0f / 1000.0f, 0.1f, 1e9f);

	void SetBody(SBody *pBody);
	void UpdateViewMatrix();
	void ProcessKeyboard(int direction, float deltaTime);
	void ProcessMouse(float xoffset, float yoffset);
};

#endif // CAMERA_H
