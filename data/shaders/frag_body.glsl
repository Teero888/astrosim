// --------------------- FRAGMENT SHADER ---------------------
#version 330 core

in vec3 ViewDir;
in vec3 WorldPos;
in vec3 Normal;

uniform vec3 LightDir;

out vec4 FragColor;

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

// vec3 CalculateAtmosphere(vec3 normal)
// {
// 	float rim = 1.0 - max(dot(ViewDir, normal), 0.0);
// 	float depth = length(WorldPos) - radius;
//
// 	// Rayleigh scattering approximation
// 	vec3 scattering = atmoColor * pow(rim, 3.0) * (1.0 + depth * 0.1);
//
// 	// Mie scattering
// 	float mie = pow(rim, 0.5) * atmoStrength * 4.0;
//
// 	return scattering * (mie + 0.5) * atmoStrength * 4.0;
// }

void main()
{
	float Diffusion = dot(LightDir, Normal);
	if(LightDir == vec3(0.0, 0.0, 0.0))
		Diffusion = 1.0;
	FragColor = vec4(vec3(Diffusion), 1.0);
}
