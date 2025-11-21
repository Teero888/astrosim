#include "camera.h"
#include "../sim/vmath.h"
#include "glm/common.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

void CCamera::SetBody(SBody *pBody)
{
	m_pFocusedBody = pBody;
	m_FocusPoint = pBody->m_SimParams.m_Position;
	m_ViewDistance = pBody->m_RenderParams.m_Radius * 5;
	m_WantedViewDistance = m_ViewDistance;

	if(m_CameraMode == MODE_FREEVIEW)
	{
		m_LocalPosition = m_AbsolutePosition - m_pFocusedBody->m_SimParams.m_Position;
		m_ViewDistance = m_LocalPosition.length();
		m_WantedViewDistance = m_ViewDistance;

		// Reset Orientation to look at planet center initially to avoid confusion
		Vec3 planetUp = m_LocalPosition.normalize();
		glm::vec3 up = (glm::vec3)planetUp;
		glm::vec3 forward = -up;
		// Reconstruct quaternion lookAt
		// Note: quatLookAt requires normalized vectors
		m_Orientation = glm::quatLookAt(forward, up);
	}
}

void CCamera::ResetCameraAngle()
{
	m_OrbitYaw = 45.0;
	m_OrbitPitch = 30.0;
	m_CameraMode = MODE_FOCUS;
	UpdateViewMatrix();
}

void CCamera::ToggleMode()
{
	if(m_CameraMode == MODE_FOCUS)
	{
		m_CameraMode = MODE_FREEVIEW;

		// Convert current Orbit state to Quaternion state
		m_Orientation = glm::quatLookAt((glm::vec3)m_Front, (glm::vec3)m_Up);
	}
	else
	{
		m_CameraMode = MODE_FOCUS;

		m_ViewDistance = m_LocalPosition.length();
		m_WantedViewDistance = m_ViewDistance;

		// Convert position back to angles
		Vec3 dir = m_LocalPosition.normalize();
		m_OrbitPitch = glm::degrees(asin(dir.y));
		m_OrbitYaw = glm::degrees(atan2(dir.z, dir.x));
	}
}

void CCamera::UpdateViewMatrix()
{
	// Projection uses massive FAR_PLANE for 1:1 scale
	m_Projection = glm::perspective(glm::radians(m_FOV), m_ScreenSize.x / m_ScreenSize.y, NEAR_PLANE, FAR_PLANE);

	if(m_CameraMode == MODE_FOCUS)
	{
		m_ViewDistance += (m_WantedViewDistance - m_ViewDistance) / ZOOM_FACTOR;

		m_OrbitPitch = glm::clamp(m_OrbitPitch, -89.0, 89.0);

		Vec3 direction;
		direction.x = cos(glm::radians(m_OrbitYaw)) * cos(glm::radians(m_OrbitPitch));
		direction.y = sin(glm::radians(m_OrbitPitch));
		direction.z = sin(glm::radians(m_OrbitYaw)) * cos(glm::radians(m_OrbitPitch));

		m_Front = -direction.normalize();
		m_Right = m_Front.cross(Vec3(0, 1, 0)).normalize();
		m_Up = m_Right.cross(m_Front).normalize();

		m_LocalPosition = direction * m_ViewDistance;
		m_AbsolutePosition = m_pFocusedBody->m_SimParams.m_Position + m_LocalPosition;

		m_View = glm::lookAt(glm::vec3(0.0f), (glm::vec3)m_Front, (glm::vec3)m_Up);
	}
	else
	{
		// == FREEVIEW QUATERNION LOGIC ==

		// 1. Update Vectors from Quaternion
		// Using standard OpenGL Identity: Front -Z, Up +Y, Right +X
		m_Front = Vec3(m_Orientation * glm::vec3(0, 0, -1));
		m_Up = Vec3(m_Orientation * glm::vec3(0, 1, 0));
		m_Right = Vec3(m_Orientation * glm::vec3(1, 0, 0));

		// 2. Update Positions
		m_AbsolutePosition = m_pFocusedBody->m_SimParams.m_Position + m_LocalPosition;
		m_ViewDistance = m_LocalPosition.length();

		// 3. Construct View Matrix from Quaternion
		// We use a rotation matrix derived from the quaternion conjugate (inverse rotation for camera)
		glm::mat4 rotate = glm::mat4_cast(glm::conjugate(m_Orientation));
		m_View = rotate;
	}
}

void CCamera::ProcessKeyboard(int direction, float deltaTime)
{
	double altitude = m_ViewDistance - m_pFocusedBody->m_RenderParams.m_Radius;
	if(altitude < 10.0)
		altitude = 10.0;

	// Dynamic speed scaling based on altitude
	double moveSpeed = altitude * 2.0 * deltaTime * m_SpeedMultiplier;

	if(m_CameraMode == MODE_FOCUS)
	{
		// Optional: Add keyboard zoom or rotate for focus mode here
	}
	else
	{
		// In Freeview, we move relative to the camera's orientation
		Vec3 oldPos = m_LocalPosition;

		if(direction == 0)
			m_LocalPosition += m_Front * moveSpeed; // W
		if(direction == 1)
			m_LocalPosition -= m_Front * moveSpeed; // S
		if(direction == 2)
			m_LocalPosition -= m_Right * moveSpeed; // A
		if(direction == 3)
			m_LocalPosition += m_Right * moveSpeed; // D
		if(direction == 4)
			m_LocalPosition += m_Up * moveSpeed; // Q (Up local)
		if(direction == 5)
			m_LocalPosition -= m_Up * moveSpeed; // E (Down local)

		// == GRAVITY ALIGNMENT MAGIC ==
		// As we move around the sphere, "Up" changes.
		// We rotate the camera orientation to match the new local Up vector.
		if(m_LocalPosition.length() > 0.1)
		{
			Vec3 oldUp = oldPos.normalize();
			Vec3 newUp = m_LocalPosition.normalize();

			// Calculate rotation from Old Gravity Normal to New Gravity Normal
			// This "drags" the camera so feet stay pointing at the planet
			glm::quat gravityRot = glm::rotation((glm::vec3)oldUp, (glm::vec3)newUp);

			// Apply this rotation to the camera globally
			m_Orientation = gravityRot * m_Orientation;
		}
	}

	UpdateViewMatrix();
}

void CCamera::ProcessMouse(float xoffset, float yoffset)
{
	if(m_CameraMode == MODE_FOCUS)
	{
		m_Sensitivity = 0.1f;
		m_OrbitYaw += xoffset * m_Sensitivity;
		m_OrbitPitch -= yoffset * m_Sensitivity;
	}
	else
	{
		// == QUATERNION ROTATION ==
		m_Sensitivity = 0.1f;
		float xRot = glm::radians(xoffset * (float)m_Sensitivity);
		float yRot = glm::radians(yoffset * (float)m_Sensitivity);

		// 1. YAW: Rotate around the PLANET'S UP vector (Global relative to planet)
		// This ensures we turn left/right relative to the ground we are standing on.
		Vec3 planetUp = m_LocalPosition.normalize();
		glm::quat yawQuat = glm::angleAxis(-xRot, (glm::vec3)planetUp);

		// 2. PITCH: Rotate around LOCAL RIGHT vector
		glm::vec3 localRight = m_Orientation * glm::vec3(1, 0, 0);

		// FIX: Removed negative sign from yRot to invert control
		glm::quat pitchQuat = glm::angleAxis(yRot, localRight);

		// Apply rotations: Pitch (Local) -> Yaw (Global) -> Orientation
		// Pitch is applied "after" (on the left) because it's relative to the current orientation
		// Yaw is applied "after" that, but relative to the planet
		m_Orientation = pitchQuat * yawQuat * m_Orientation;
		m_Orientation = glm::normalize(m_Orientation);
	}
	UpdateViewMatrix();
}
