#include "camera.h"
#include "glm/common.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include <cmath>
#include <cstdio>

void CCamera::SetBody(SBody *pBody)
{
	m_pFocusedBody = pBody;
	m_FocusPoint = pBody->m_SimParams.m_Position;
}

// TODO: might want to add delta time here for the radius interpolation
void CCamera::UpdateViewMatrix()
{
	switch(m_CameraMode)
	{
	case MODE_FOCUS:
	{
		m_ViewDistance += (m_WantedViewDistance - m_ViewDistance) / ZOOM_FACTOR;

		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

		m_Position.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		m_Position.y = sin(glm::radians(m_Pitch));
		m_Position.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

		m_Front = glm::normalize(-m_Position);
		m_Right = glm::normalize(glm::cross(m_Front, m_Up));

		Vec3 orbitOffset = Vec3(m_Position) * m_ViewDistance;
		m_AbsolutePosition = m_pFocusedBody->m_SimParams.m_Position + orbitOffset;

		glm::mat4 rotation = glm::lookAt(glm::vec3(0.0f), m_Front, m_Up);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3((float)(DEFAULT_SCALE / m_ViewDistance)));
		m_View = rotation * scale;
		break;
	}

	// TODO: this mode doesn't actually work lol fix this once i need it
	case MODE_FREEVIEW:
	{
		glm::mat4 rotation = glm::lookAt(glm::vec3(0.0f), m_Front, m_Up);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3((float)(DEFAULT_SCALE / m_ViewDistance)));
		m_View = rotation * scale;
		break;
	}
	}
}

// TODO: unused currently this will be for the freeview mode (idk maybe do spaceships, would be fun)
void CCamera::ProcessKeyboard(int direction, float deltaTime)
{
	float velocity = m_Speed * deltaTime;
	if(direction == 0) // Forward
		m_Position += m_Front * velocity;
	if(direction == 1) // Backward
		m_Position -= m_Front * velocity;
	if(direction == 2) // Left
		m_Position -= m_Right * velocity;
	if(direction == 3) // Right
		m_Position += m_Right * velocity;

	UpdateViewMatrix();
}

void CCamera::ProcessMouse(float xoffset, float yoffset)
{
	m_Sensitivity = fmin((m_ViewDistance - m_pFocusedBody->m_RenderParams.m_Radius) / m_pFocusedBody->m_RenderParams.m_Radius, 0.1); // [NEW]
	// printf("sens: %f\n", m_Sensitivity);

	xoffset *= m_Sensitivity;
	yoffset *= m_Sensitivity;

	switch(m_CameraMode)
	{
	// TODO: will be changed dw
	case MODE_FREEVIEW:
	{
		m_Yaw += xoffset;
		m_Pitch += yoffset;
		break;
	}

	case MODE_FOCUS:
	{
		m_Yaw += xoffset;
		m_Pitch -= yoffset;
		break;
	}
	}
	UpdateViewMatrix();
}
