#ifndef CAMERA_H
#define CAMERA_H

#include "../sim/body.h"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include <glm/gtc/quaternion.hpp> // Added
#include <glm/gtc/type_ptr.hpp>

#define ZOOM_FACTOR 5.0
#define FAR_PLANE 1e30f
#define NEAR_PLANE 0.1f

enum ECameraMode
{
	MODE_FOCUS = 0,
	MODE_FREEVIEW,
};

struct CCamera
{
	SBody *m_pFocusedBody = nullptr;
	Vec3 m_AbsolutePosition;
	Vec3 m_LocalPosition;

	double m_ViewDistance = 20000.0;
	double m_WantedViewDistance = 20000.0;

	glm::vec2 m_ScreenSize;
	Vec3 m_FocusPoint = Vec3(0, 0, 0);
	ECameraMode m_CameraMode = MODE_FOCUS;

	// Orientation (Quaternion based)
	glm::quat m_Orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity

	// Cached vectors for movement
	Vec3 m_Front = Vec3(0.0f, 0.0f, -1.0f);
	Vec3 m_Up = Vec3(0.0f, 1.0f, 0.0f);
	Vec3 m_Right = Vec3(1.0f, 0.0f, 0.0f);

	// Focus Mode Angles (Still useful for orbit cam)
	double m_OrbitYaw = 45.0f;
	double m_OrbitPitch = 30.0f;

	double m_Speed = 100.0f;
	float m_SpeedMultiplier = 1.0f;
	double m_Sensitivity = 0.1f;

	float m_FOV = 70.f;
	glm::mat4 m_View;
	glm::mat4 m_Projection;

	int m_LOD = 0;

	void SetBody(SBody *pBody);
	void ToggleMode();
	void ResetCameraAngle();
	void UpdateViewMatrix();
	void ProcessKeyboard(int direction, float deltaTime);
	void ProcessMouse(float xoffset, float yoffset);
};

#endif // CAMERA_H
