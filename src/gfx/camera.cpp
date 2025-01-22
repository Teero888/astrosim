#include "camera.h"
#include "glm/ext/vector_float3.hpp"

void CCamera::UpdateViewMatrix()
{
	switch(m_CameraMode)
	{
	case MODE_FOCUS:
	{
		// Clamp pitch
		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

		// Calculate new position using spherical coordinates
		m_Position.x = m_Radius * cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		m_Position.y = m_Radius * sin(glm::radians(m_Pitch));
		m_Position.z = m_Radius * sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

		// Update front/right vectors to face focus point
		m_Front = glm::normalize(-m_Position);
		m_Right = glm::normalize(glm::cross(m_Front, m_Up));

		m_View = glm::lookAt(m_Position, {0, 0, 0}, m_Up);
		break;
	}
	case MODE_FREEVIEW:
	{
		m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
		break;
	}
	}
}

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
	xoffset *= m_Sensitivity;
	yoffset *= m_Sensitivity;

	switch(m_CameraMode)
	{
	case MODE_FREEVIEW:
	{
		// Existing free-view rotation
		m_Yaw += xoffset;
		m_Pitch += yoffset;

		// Clamp pitch
		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
		break;
	}

	case MODE_FOCUS:
	{
		// Orbit around focus point
		m_Yaw += xoffset;
		m_Pitch -= yoffset; // Invert Y-axis for intuitive orbit

		// Clamp pitch
		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
		break;
	}
	}
	UpdateViewMatrix();
}
