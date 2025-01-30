// --------------------- VERTEX SHADER ---------------------
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform float Roughness;
uniform float Radius;
uniform vec3 CameraPos;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 ViewDir;
out vec3 WorldPos;
out vec3 Normal;

vec4 hash(vec4 p)
{
	p = vec4(
		dot(p, vec4(127.1, 311.7, 74.7, 321.3)),
		dot(p, vec4(269.5, 183.3, 246.1, 456.7)),
		dot(p, vec4(113.5, 271.9, 124.6, 789.2)),
		dot(p, vec4(368.4, 198.3, 155.2, 159.7)));
	return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noise(vec4 p)
{
	vec4 i = floor(p);
	vec4 f = fract(p);

// Calculate interpolation factors
#if INTERPOLANT == 1
	// Quintic interpolant
	vec4 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
#else
	// Cubic interpolant
	vec4 u = f * f * (3.0 - 2.0 * f);
#endif

	// Calculate contributions from both w layers
	float w0 = mix(
		mix(
			mix(
				mix(dot(hash(i + vec4(0, 0, 0, 0)), f - vec4(0, 0, 0, 0)),
					dot(hash(i + vec4(1, 0, 0, 0)), f - vec4(1, 0, 0, 0)), u.x),
				mix(dot(hash(i + vec4(0, 1, 0, 0)), f - vec4(0, 1, 0, 0)),
					dot(hash(i + vec4(1, 1, 0, 0)), f - vec4(1, 1, 0, 0)), u.x),
				u.y),
			mix(
				mix(dot(hash(i + vec4(0, 0, 1, 0)), f - vec4(0, 0, 1, 0)),
					dot(hash(i + vec4(1, 0, 1, 0)), f - vec4(1, 0, 1, 0)), u.x),
				mix(dot(hash(i + vec4(0, 1, 1, 0)), f - vec4(0, 1, 1, 0)),
					dot(hash(i + vec4(1, 1, 1, 0)), f - vec4(1, 1, 1, 0)), u.x),
				u.y),
			u.z),
		mix(
			mix(
				mix(dot(hash(i + vec4(0, 0, 0, 1)), f - vec4(0, 0, 0, 1)),
					dot(hash(i + vec4(1, 0, 0, 1)), f - vec4(1, 0, 0, 1)), u.x),
				mix(dot(hash(i + vec4(0, 1, 0, 1)), f - vec4(0, 1, 0, 1)),
					dot(hash(i + vec4(1, 1, 0, 1)), f - vec4(1, 1, 0, 1)), u.x),
				u.y),
			mix(
				mix(dot(hash(i + vec4(0, 0, 1, 1)), f - vec4(0, 0, 1, 1)),
					dot(hash(i + vec4(1, 0, 1, 1)), f - vec4(1, 0, 1, 1)), u.x),
				mix(dot(hash(i + vec4(0, 1, 1, 1)), f - vec4(0, 1, 1, 1)),
					dot(hash(i + vec4(1, 1, 1, 1)), f - vec4(1, 1, 1, 1)), u.x),
				u.y),
			u.z),
		u.w);

	return w0;
}

void main()
{
	ViewDir = normalize(vec3(Model * vec4(CameraPos, 1.0)));

	vec4 Coord = vec4(aPos * 10.0, 0.0);
	float Height = (1.0 - (noise(Coord * 2.0) * 0.5 + 1.0)) * Roughness;
	WorldPos = vec3(Model * vec4(aPos * vec3(Radius) * (1.0 + Height), 1.0));

	Normal = normalize(aPos);

	gl_Position = Projection * View * vec4(WorldPos, 1.0);
}
