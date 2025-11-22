#include "camera.h"
#include "../sim/vmath.h"
#include "glm/common.hpp"
#include "glm/ext/vector_float3.hpp"
#include "graphics.h"
#include "imgui.h"
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
		Vec3 WorldOffset = m_AbsolutePosition - m_pFocusedBody->m_SimParams.m_Position;

		Vec3 PlanetUp = WorldOffset.normalize();
		glm::vec3 Up = (glm::vec3)PlanetUp;
		glm::vec3 Forward = -Up;
		glm::quat WorldOrientation = glm::quatLookAt(Forward, Up);

		if(m_RotateWithBody)
		{
			Quat q = m_pFocusedBody->m_SimParams.m_Orientation;
			glm::quat PlanetRot(q.w, q.x, q.y, q.z);
			glm::quat PlanetRotInv = glm::inverse(PlanetRot);

			m_LocalPosition = q.Conjugate().RotateVector(WorldOffset);
			m_Orientation = PlanetRotInv * WorldOrientation;
		}
		else
		{
			m_LocalPosition = WorldOffset;
			m_Orientation = WorldOrientation;
		}

		m_ViewDistance = m_LocalPosition.length();
		m_WantedViewDistance = m_ViewDistance;
	}
}

void CCamera::SetBodyRotationMode(bool bEnable)
{
	if(m_RotateWithBody == bEnable || !m_pFocusedBody)
		return;

	Quat q = m_pFocusedBody->m_SimParams.m_Orientation;
	glm::quat PlanetRot(q.w, q.x, q.y, q.z);
	glm::quat PlanetRotInv = glm::inverse(PlanetRot);

	if(bEnable)
	{
		m_LocalPosition = q.Conjugate().RotateVector(m_LocalPosition);
		if(m_CameraMode == MODE_FREEVIEW)
			m_Orientation = PlanetRotInv * m_Orientation;
	}
	else
	{
		m_LocalPosition = q.RotateVector(m_LocalPosition);
		if(m_CameraMode == MODE_FREEVIEW)
			m_Orientation = PlanetRot * m_Orientation;
	}

	if(m_CameraMode == MODE_FOCUS)
	{
		Vec3 Dir = m_LocalPosition.normalize();
		double yClamped = glm::clamp(Dir.y, -1.0, 1.0);
		m_OrbitPitch = glm::degrees(asin(yClamped));
		m_OrbitYaw = glm::degrees(atan2(Dir.z, Dir.x));
	}

	m_RotateWithBody = bEnable;
	UpdateViewMatrix();
}

void CCamera::UpdateViewMatrix()
{
	if(!m_pFocusedBody)
		return;

	m_Projection = glm::perspective(glm::radians(m_FOV), m_ScreenSize.x / m_ScreenSize.y, NEAR_PLANE, FAR_PLANE);

	glm::quat PlanetRot = glm::quat(1.0, 0.0, 0.0, 0.0);
	Quat q = Quat::Identity();

	if(m_pFocusedBody)
	{
		q = m_pFocusedBody->m_SimParams.m_Orientation;
		PlanetRot = glm::quat(q.w, q.x, q.y, q.z);
	}

	if(m_CameraMode == MODE_FOCUS)
	{
		m_ViewDistance += (m_WantedViewDistance - m_ViewDistance) / ZOOM_FACTOR;
		m_OrbitPitch = glm::clamp(m_OrbitPitch, -89.0, 89.0);

		Vec3 LocalDir;
		LocalDir.x = cos(glm::radians(m_OrbitYaw)) * cos(glm::radians(m_OrbitPitch));
		LocalDir.y = sin(glm::radians(m_OrbitPitch));
		LocalDir.z = sin(glm::radians(m_OrbitYaw)) * cos(glm::radians(m_OrbitPitch));

		Vec3 LocalFront = -LocalDir.normalize();
		Vec3 LocalRight = LocalFront.cross(Vec3(0, 1, 0)).normalize();
		Vec3 LocalUp = LocalRight.cross(LocalFront).normalize();

		if(m_RotateWithBody)
		{
			m_Front = Vec3(PlanetRot * (glm::vec3)LocalFront);
			m_Up = Vec3(PlanetRot * (glm::vec3)LocalUp);
			m_Right = Vec3(PlanetRot * (glm::vec3)LocalRight);

			Vec3 WorldOffset = q.RotateVector(LocalDir * m_ViewDistance);
			m_AbsolutePosition = m_pFocusedBody->m_SimParams.m_Position + WorldOffset;

			m_LocalPosition = LocalDir * m_ViewDistance;
		}
		else
		{
			m_Front = LocalFront;
			m_Up = LocalUp;
			m_Right = LocalRight;

			m_LocalPosition = LocalDir * m_ViewDistance;
			m_AbsolutePosition = m_pFocusedBody->m_SimParams.m_Position + m_LocalPosition;
		}

		m_View = glm::lookAt(glm::vec3(0.0f), (glm::vec3)m_Front, (glm::vec3)m_Up);
	}
	else // MODE_FREEVIEW
	{
		glm::quat WorldOrientation;
		Vec3 WorldPos;

		if(m_RotateWithBody)
		{
			// m_Orientation is in BODY SPACE.
			// m_LocalPosition is in BODY SPACE.

			WorldOrientation = PlanetRot * m_Orientation;
			Vec3 worldOffset = q.RotateVector(m_LocalPosition);
			WorldPos = m_pFocusedBody->m_SimParams.m_Position + worldOffset;
		}
		else
		{
			// m_Orientation is in WORLD SPACE.
			// m_LocalPosition is in WORLD SPACE (offset).
			WorldOrientation = m_Orientation;
			WorldPos = m_pFocusedBody->m_SimParams.m_Position + m_LocalPosition;
		}

		m_Front = Vec3(WorldOrientation * glm::vec3(0, 0, -1));
		m_Up = Vec3(WorldOrientation * glm::vec3(0, 1, 0));
		m_Right = Vec3(WorldOrientation * glm::vec3(1, 0, 0));

		m_AbsolutePosition = WorldPos;
		m_ViewDistance = m_LocalPosition.length();

		m_View = glm::mat4_cast(glm::conjugate(WorldOrientation));
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

		// create world space orientation from current view
		glm::quat worldOrientation = glm::quatLookAt((glm::vec3)m_Front, (glm::vec3)m_Up);

		if(m_RotateWithBody && m_pFocusedBody)
		{
			// convert world orientation -> body space orientation
			Quat q = m_pFocusedBody->m_SimParams.m_Orientation;
			glm::quat planetRot(q.w, q.x, q.y, q.z);

			m_Orientation = glm::inverse(planetRot) * worldOrientation;
		}
		else
			m_Orientation = worldOrientation;
	}
	else
	{
		m_CameraMode = MODE_FOCUS;

		m_ViewDistance = m_LocalPosition.length();
		m_WantedViewDistance = m_ViewDistance;

		Vec3 Dir = m_LocalPosition.normalize();
		m_OrbitPitch = glm::degrees(asin(Dir.y));
		m_OrbitYaw = glm::degrees(atan2(Dir.z, Dir.x));
	}
}

void CCamera::ProcessKeyboard(float DeltaTime)
{
	double Altitude = m_ViewDistance - m_pFocusedBody->m_RenderParams.m_Radius;
	if(Altitude < 10.0)
		Altitude = 10.0;

	// speed scaling based on altitude
	double MoveSpeed = Altitude * 2.0 * DeltaTime * m_SpeedMultiplier;

	switch(m_CameraMode)
	{
	case MODE_FOCUS:
		break;

	case MODE_FREEVIEW:
	{
		Vec3 OldPos = m_LocalPosition;
		Vec3 MoveFront, MoveRight, MoveUp;

		if(m_RotateWithBody)
		{
			MoveFront = Vec3(m_Orientation * glm::vec3(0, 0, -1));
			MoveRight = Vec3(m_Orientation * glm::vec3(1, 0, 0));
			MoveUp = Vec3(m_Orientation * glm::vec3(0, 1, 0));
		}
		else
		{
			// m_Orientation is World Space. m_Front/Right/Up are World Space.
			MoveFront = m_Front;
			MoveRight = m_Right;
			MoveUp = m_Up;
		}

		if(ImGui::IsKeyDown(ImGuiKey_W))
			m_LocalPosition += MoveFront * MoveSpeed; // W
		if(ImGui::IsKeyDown(ImGuiKey_S))
			m_LocalPosition -= MoveFront * MoveSpeed; // S
		if(ImGui::IsKeyDown(ImGuiKey_A))
			m_LocalPosition -= MoveRight * MoveSpeed; // A
		if(ImGui::IsKeyDown(ImGuiKey_D))
			m_LocalPosition += MoveRight * MoveSpeed; // D
		if(ImGui::IsKeyDown(ImGuiKey_Q))
			m_LocalPosition += MoveUp * MoveSpeed; // Q (Up local)
		if(ImGui::IsKeyDown(ImGuiKey_E))
			m_LocalPosition -= MoveUp * MoveSpeed; // E (Down local)

		if(m_LocalPosition.length() > 0.1)
		{
			Vec3 OldUp = OldPos.normalize();
			Vec3 NewUp = m_LocalPosition.normalize();
			glm::quat GravityRot = glm::rotation((glm::vec3)OldUp, (glm::vec3)NewUp);
			m_Orientation = GravityRot * m_Orientation;
		}
	}
	}

	UpdateViewMatrix();
}

void CCamera::ProcessMouse(float xoffset, float yoffset)
{
	switch(m_CameraMode)
	{
	case MODE_FOCUS:
	{
		m_Sensitivity = 0.1f;
		m_OrbitYaw += xoffset * m_Sensitivity;
		m_OrbitPitch -= yoffset * m_Sensitivity;
		break;
	}
	case MODE_FREEVIEW:
	{
		m_Sensitivity = 0.1f;
		float XRot = glm::radians(xoffset * (float)m_Sensitivity);
		float YRot = glm::radians(yoffset * (float)m_Sensitivity);
		Vec3 PlanetUp = m_LocalPosition.normalize();
		glm::quat YawQuat = glm::angleAxis(-XRot, (glm::vec3)PlanetUp);
		glm::vec3 LocalRight = m_Orientation * glm::vec3(1, 0, 0);
		glm::quat PitchQuat = glm::angleAxis(YRot, LocalRight);
		m_Orientation = PitchQuat * YawQuat * m_Orientation;
		m_Orientation = glm::normalize(m_Orientation);
		break;
	}
	}
	UpdateViewMatrix();
}
