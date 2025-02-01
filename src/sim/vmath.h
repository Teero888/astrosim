#ifndef VMATH_H
#define VMATH_H
#include "glm/ext/vector_float3.hpp"
#include <cmath>
#include <glm/glm.hpp>

struct Vec3
{
	double x, y, z;

	Vec3() :
		x(0), y(0), z(0) {}
	Vec3(double x, double y, double z) :
		x(x), y(y), z(z) {}
	Vec3(double v) :
		x(v), y(v), z(v) {}
	Vec3(glm::vec3 v) :
		x(v.x), y(v.y), z(v.z) {}

	Vec3 operator+(const Vec3 &other) const
	{
		return Vec3(x + other.x, y + other.y, z + other.z);
	}

	Vec3 operator-(const Vec3 &other) const
	{
		return Vec3(x - other.x, y - other.y, z - other.z);
	}

	Vec3 operator/(const Vec3 &other) const
	{
		return Vec3(x / other.x, y / other.y, z / other.z);
	}

	Vec3 operator*(const Vec3 &other) const
	{
		return Vec3(x * other.x, y * other.y, z * other.z);
	}

	Vec3 operator-(double sub) const
	{
		return Vec3(x - sub, y - sub, z - sub);
	}

	Vec3 operator+(double add) const
	{
		return Vec3(x - add, y - add, z - add);
	}

	Vec3 operator*(double scalar) const
	{
		return Vec3(x * scalar, y * scalar, z * scalar);
	}

	Vec3 operator/(double scalar) const
	{
		return Vec3(x / scalar, y / scalar, z / scalar);
	}

	Vec3 operator%(double mod) const
	{
		return Vec3(fmod(x, mod), fmod(y, mod), fmod(z, mod));
	}

	operator glm::vec3() const
	{
		return glm::vec3(x, y, z);
	}

	inline double length() const { return std::sqrt(x * x + y * y + z * z); }
	inline Vec3 floor() const { return Vec3((int)x, (int)y, (int)z); }
};

inline glm::vec2 WorldToScreenCoordinates(glm::vec3 WorldPos, glm::mat4 Model, glm::mat4 View, glm::mat4 Projection, int ScreenWidth, int ScreenHeight)
{
	glm::mat4 mvp = Projection * View * Model;
	glm::vec4 clipSpacePos = mvp * glm::vec4(WorldPos, 1.0f);
	// outside of camera view
	if(clipSpacePos.w < 0.f)
		return glm::vec2(-1.f);
	glm::vec3 ndcPos = glm::vec3(clipSpacePos) / clipSpacePos.w;
	glm::vec2 screenPos;
	screenPos.x = (ndcPos.x + 1.0f) * 0.5f * ScreenWidth;
	screenPos.y = (1.0f - ndcPos.y) * 0.5f * ScreenHeight;
	return screenPos;
}

#endif // VMATH_H
