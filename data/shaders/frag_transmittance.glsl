#version 460 core
out vec2 FragColor; // R = Rayleigh Optical Depth, G = Mie Optical Depth

// Inputs matching the main atmosphere logic
uniform float u_atmosphereRadius;
uniform float u_rayleighScaleHeight;
uniform float u_mieScaleHeight;

const int STEPS = 500; // High quality integration step count
const float EARTH_R = 1.0; // Unit space planet radius

float GetDensity(float h, float H)
{
	return exp(-max(0.0, h) / H);
}

// Optimized Intersection
vec2 raySphereIntersect(vec3 ro, vec3 rd, float rad)
{
	float b = dot(ro, rd);
	float c = dot(ro, ro) - rad * rad;
	float d = b * b - c;
	if(d < 0.0)
		return vec2(1e5, -1e5);
	float sqrtD = sqrt(d);
	return vec2(-b - sqrtD, -b + sqrtD);
}

void main()
{
	// Texture Resolution is typically 512x512
	// gl_FragCoord is [0.5, 511.5]
	vec2 uv = gl_FragCoord.xy / vec2(512.0, 512.0);

	// Decode UV to Physics Parameters

	// X-Axis: Sun Angle (CosTheta)
	// Map [0, 1] -> [-1, 1]
	float cosTheta = uv.x * 2.0 - 1.0;
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));

	// Y-Axis: Height
	// Map [0, 1] -> [Surface, Top of Atmosphere]
	float height = uv.y * (u_atmosphereRadius - EARTH_R);
	float r = EARTH_R + height;

	// Setup Ray
	// Start at (0, r, 0)
	vec3 startPos = vec3(0.0, r, 0.0);
	// Direction based on angle theta relative to Up (0, 1, 0)
	vec3 rayDir = vec3(sinTheta, cosTheta, 0.0);

	// Intersect Atmosphere
	vec2 hit = raySphereIntersect(startPos, rayDir, u_atmosphereRadius);

	// We only care about the exit point (hit.y)
	// Ideally startPos is inside, so hit.x is negative behind us, hit.y is positive forward
	float dist = hit.y;

	// Integrate
	float stepSize = dist / float(STEPS);
	float accumR = 0.0;
	float accumM = 0.0;

	// Safety: Check if ray hits the planet itself (horizon occlusion)
	// If CosTheta is negative, we might hit the ground
	if(cosTheta < 0.0)
	{
		vec2 planetHit = raySphereIntersect(startPos, rayDir, EARTH_R);
		if(planetHit.y > 0.0 && planetHit.x > 0.0)
		{
			// Ray hits planet: Light is blocked. Infinite optical depth.
			FragColor = vec2(1e9, 1e9);
			return;
		}
	}

	for(int i = 0; i < STEPS; i++)
	{
		vec3 pos = startPos + rayDir * (float(i) + 0.5) * stepSize;
		float h = length(pos) - EARTH_R;

		accumR += GetDensity(h, u_rayleighScaleHeight) * stepSize;
		accumM += GetDensity(h, u_mieScaleHeight) * stepSize;
	}

	FragColor = vec2(accumR, accumM);
}
