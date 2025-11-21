#ifndef QMATH_H
#define QMATH_H

#include "vmath.h"
#include <cmath>

struct Quat
{
	double w, x, y, z;

	Quat() :
		w(1.0), x(0.0), y(0.0), z(0.0) {}
	Quat(double _w, double _x, double _y, double _z) :
		w(_w), x(_x), y(_y), z(_z) {}

	// Identity
	static Quat Identity() { return Quat(1.0, 0.0, 0.0, 0.0); }

	// Construct from Axis-Angle (Angle in degrees)
	static Quat FromAxisAngle(Vec3 axis, double angleDeg)
	{
		double halfAngle = glm::radians(angleDeg) * 0.5;
		double s = std::sin(halfAngle);
		Vec3 n = axis.normalize();
		return Quat(std::cos(halfAngle), n.x * s, n.y * s, n.z * s);
	}

	// Normalize to prevent drift
	void Normalize()
	{
		double mag = std::sqrt(w * w + x * x + y * y + z * z);
		if(mag > 0.0)
		{
			double inv = 1.0 / mag;
			w *= inv;
			x *= inv;
			y *= inv;
			z *= inv;
		}
		else
		{
			*this = Identity();
		}
	}

	// Conjugate (Inverse rotation for unit quaternions)
	Quat Conjugate() const
	{
		return Quat(w, -x, -y, -z);
	}

	// Quaternion Multiplication
	Quat operator*(const Quat &q) const
	{
		return Quat(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y - x * q.z + y * q.w + z * q.x,
			w * q.z + x * q.y - y * q.x + z * q.w);
	}

	// Rotate Vector v by this quaternion
	// formula: v' = q * v * q_inv
	Vec3 RotateVector(const Vec3 &v) const
	{
		// Optimization: 2.0 * cross(q.xyz, v * q.w + cross(q.xyz, v)) + v
		Vec3 qv(x, y, z);
		Vec3 uv = qv.cross(v);
		Vec3 uuv = qv.cross(uv);
		return v + ((uv * w) + uuv) * 2.0;
	}
};

// Integration Helper: Integrate angular velocity over dt
// q_new = q_old + 0.5 * omega * q_old * dt
inline void IntegrateRotation(Quat &orientation, const Vec3 &angularVelocity, double dt)
{
	// Create a pure quaternion from angular velocity (w=0)
	Quat omega(0.0, angularVelocity.x, angularVelocity.y, angularVelocity.z);

	// Spin derivative = 0.5 * omega * orientation
	Quat spin = omega * orientation;

	orientation.w += spin.w * 0.5 * dt;
	orientation.x += spin.x * 0.5 * dt;
	orientation.y += spin.y * 0.5 * dt;
	orientation.z += spin.z * 0.5 * dt;

	orientation.Normalize();
}

#endif // QMATH_H
