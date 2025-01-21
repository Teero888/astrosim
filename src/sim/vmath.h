#ifndef VMATH_H
#define VMATH_H
#include <cmath>

struct Vec3
{
	double x, y, z;

	Vec3() :
		x(0), y(0), z(0) {}
	Vec3(double x, double y, double z) :
		x(x), y(y), z(z) {}

	Vec3 operator+(const Vec3 &other) const
	{
		return Vec3(x + other.x, y + other.y, z + other.z);
	}

	Vec3 operator-(const Vec3 &other) const
	{
		return Vec3(x - other.x, y - other.y, z - other.z);
	}

	Vec3 operator*(double scalar) const
	{
		return Vec3(x * scalar, y * scalar, z * scalar);
	}

	Vec3 operator/(double scalar) const
	{
		return Vec3(x / scalar, y / scalar, z / scalar);
	}

	double length() const { return std::sqrt(x * x + y * y + z * z); }
};

#endif // VMATH_H
