// --------------------- VERTEX SHADER ---------------------
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform vec3 cameraPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float radius;
uniform float time;
uniform float textureScale;
uniform float rotationSpeed;
uniform float axialTilt;
uniform float roughness;

uniform vec3 lightPos;

out vec3 ViewDir;
out vec3 WorldPos;
out vec3 Normal;
out vec3 LocalPos;
out vec3 TangentLightDir;

const float PI = 3.14159265359;

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

float fbm(vec4 p, int octaves)
{
	float value = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;

	for(int i = 0; i < octaves; i++)
	{
		value += amplitude * noise(p * frequency);
		frequency *= 2.0;
		amplitude *= 0.5;
		p += vec4(12.345, 67.890, 123.456, 789.012);
	}
	return value;
}

void main()
{
	// Apply axial tilt
	float tiltRad = radians(axialTilt);
	mat3 tiltMatrix = mat3(
		1, 0, 0,
		0, cos(tiltRad), -sin(tiltRad),
		0, sin(tiltRad), cos(tiltRad));

	vec3 tiltedPos = tiltMatrix * aPos;

	ViewDir = vec3(model * vec4(cameraPos, 1.0));
	LocalPos = tiltedPos;

	// Apply elevation
	{
		vec4 coord = vec4(LocalPos * textureScale, time * rotationSpeed * 0.01);
		float height = (1.0 - (fbm(coord * 2.0, 5) * 0.5 + 1.0)) * roughness;

		WorldPos = vec3(model * vec4(tiltedPos * radius, 1.0));
		WorldPos *= 1.0 + height;
	}

	// Calculate TBN matrix
	vec3 T = normalize(vec3(tiltMatrix * vec3(1, 0, 0)));
	vec3 B = normalize(vec3(tiltMatrix * vec3(0, 1, 0)));
	vec3 N = normalize(vec3(tiltMatrix * aNormal));
	mat3 TBN = mat3(T, B, N);

	// Transform light direction to tangent space
	vec3 lightDir = normalize(lightPos - WorldPos);
	TangentLightDir = lightDir * TBN;

	Normal = N;
	gl_Position = projection * view * vec4(WorldPos, 1.0);
}
