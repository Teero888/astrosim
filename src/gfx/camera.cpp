#include "camera.h"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"

// TODO: might want to add delta time here for the radius interpolation
void CCamera::UpdateViewMatrix()
{
	switch(m_CameraMode)
	{
	case MODE_FOCUS:
	{
		m_Radius += (m_WantedRadius - m_Radius) / ZOOM_FACTOR;

		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);

		m_Position.x = DEFAULT_SCALE * cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		m_Position.y = DEFAULT_SCALE * sin(glm::radians(m_Pitch));
		m_Position.z = DEFAULT_SCALE * sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

		m_Front = glm::normalize(-m_Position);
		m_Right = glm::normalize(glm::cross(m_Front, m_Up));

		m_View = glm::lookAt(m_Position, {0, 0, 0}, m_Up);
		break;
	}

	// TODO: this mode doesn't actually work lol fix this once i need it
	case MODE_FREEVIEW:
	{
		m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
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
	xoffset *= m_Sensitivity;
	yoffset *= m_Sensitivity;

	switch(m_CameraMode)
	{
	// TODO: will be changed dw
	case MODE_FREEVIEW:
	{
		m_Yaw += xoffset;
		m_Pitch += yoffset;

		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
		break;
	}

	case MODE_FOCUS:
	{
		m_Yaw += xoffset;
		m_Pitch -= yoffset;

		m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
		break;
	}
	}
	UpdateViewMatrix();
}
