#version 330 core
out vec4 FragColor;
in vec2 uv;

uniform vec3 LightDir;
uniform vec3 CameraPos;

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

float sdSphere(vec3 p, float radius)
{
	float sdf = 0.0;
	radius += noise(vec4(p * 3.0, 0.0)) * 0.1;
	return (length(p) - radius) * 0.5;
}

mat3 GetCam(vec3 ro, vec3 lookAt)
{
	vec3 camF = normalize(vec3(lookAt - ro));
	vec3 camR = normalize(cross(vec3(0, 1, 0), camF));
	vec3 camU = cross(camF, camR);
	return mat3(camR, camU, camF);
}

vec3 CalcNormal(vec3 p, float radius)
{
	const float eps = 0.001;
	return normalize(vec3(
		sdSphere(p + vec3(eps, 0, 0), radius) - sdSphere(p - vec3(eps, 0, 0), radius),
		sdSphere(p + vec3(0, eps, 0), radius) - sdSphere(p - vec3(0, eps, 0), radius),
		sdSphere(p + vec3(0, 0, eps), radius) - sdSphere(p - vec3(0, 0, eps), radius)));
}

#define BODY_RADIUS 0.5

#define MAX_DIST 1.
#define MAX_ITER 100.
#define EPSILON .001
void main()
{
	vec3 ro = vec3(CameraPos.xy, -CameraPos.z); // Ray origin
	vec3 rd = GetCam(ro, vec3(0.0)) * normalize(vec3(uv, 1.0)); // Ray direction

	// Raymarching params
	vec4 color = vec4(0.0);
	int i = 0;
	float t = 0.0;
	for(; i < MAX_ITER; i++)
	{
		vec3 p = ro + rd * t;
		float distance = sdSphere(p, BODY_RADIUS);

		if(distance < EPSILON)
		{
			vec3 normal = CalcNormal(ro + rd * t, BODY_RADIUS);
			color = vec4(normal, 1.0);
			break;
		}

		if(t > MAX_DIST)
			break;

		t += distance;
	}
	FragColor = color;
}
