#version 460 core

in vec3 v_rayDirection;
in vec3 v_viewRay;
out vec4 FragColor;

uniform sampler2D u_depthTexture;

uniform vec2 u_screenSize;

uniform vec3 u_sunDirection;
uniform vec3 u_cameraPos; // Unit Space (Relative to Planet Center)
uniform float u_atmosphereRadius; // Meters
uniform float u_realPlanetRadius; // Meters

// Physics
uniform vec3 u_rayleighScatteringCoeff;
uniform float u_rayleighScaleHeight;
uniform vec3 u_mieScatteringCoeff;
uniform float u_mieScaleHeight;
uniform float u_miePreferredScatteringDir;
uniform float u_sunIntensity;
uniform float u_logDepthF;

const int NUM_SAMPLES = 24;      // Primary Ray Steps
const int NUM_SUN_SAMPLES = 4;   // Secondary Ray Steps (Light Shafts)
const float PI = 3.141592653589793;

// == INTERSECTIONS (Unit Space) ==

vec2 raySphereIntersect(vec3 ro, vec3 rd, float rad)
{
	float b = dot(ro, rd);
	float c = dot(ro, ro) - rad * rad;
	float d = b * b - c;
	if(d < 0.0) return vec2(1e5, -1e5);
	float sqrtD = sqrt(d);
	return vec2(-b - sqrtD, -b + sqrtD);
}

// Reconstruct World Position (Meters, Camera Relative)
vec3 ReconstructWorldPos(float logZ, vec2 uv)
{
	float viewZ = exp2(logZ / (u_logDepthF * 0.5)) - 1.0;
	vec3 rayDirView = normalize(v_viewRay);
	return rayDirView * (-viewZ / rayDirView.z);
}

float RayleighPhase(float mu) { return 3.0 / (16.0 * PI) * (1.0 + mu * mu); }
float MiePhase(float mu, float g) { return 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5)); }

float GetDensity(float h, float H)
{
	// Clamp h to 0 to ensure valleys are filled with dense air (not vacuum)
	return exp(-max(0.0, h) / H);
}

// ANALYTIC OPTICAL DEPTH INTEGRATION
// Calculates how much air is between 'p' and the sun.
// Replaces the LUT lookup for perfect horizon gradients.
vec2 GetSunTransmittance(vec3 p, vec3 sunDir, float atmRadius, float earthRadius, float H_R, float H_M)
{
	vec2 intersect = raySphereIntersect(p, sunDir, atmRadius);
	float distToTop = intersect.y;
	
	// Optimization: If we are intersecting the planet while looking at the sun, 
	// we are in shadow. The analytic sphere check in main() handles this, 
	// so we assume here we have a clear line of sight to the atmosphere top.
	
	float stepSize = distToTop / float(NUM_SUN_SAMPLES);
	float halfStep = stepSize * 0.5;
	
	float optR = 0.0;
	float optM = 0.0;
	
	for(int i = 0; i < NUM_SUN_SAMPLES; i++)
	{
		vec3 pos = p + sunDir * (float(i) * stepSize + halfStep);
		float h = length(pos) - earthRadius;
		
		optR += GetDensity(h, H_R);
		optM += GetDensity(h, H_M);
	}
	
	return vec2(optR * stepSize, optM * stepSize);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / u_screenSize;
	
	// 1. Depth Reconstruction
	// Use texelFetch to match pixel centers exactly
	float depthVal = texelFetch(u_depthTexture, ivec2(gl_FragCoord.xy), 0).r;

	// 2. Constants (Unit Space)
	float earthR = 1.0;
	float atmR = u_atmosphereRadius / u_realPlanetRadius;
	vec3 rayOrigin = u_cameraPos;
	vec3 rayDir = normalize(v_rayDirection);

	// 3. Atmosphere Intersection
	vec2 hitInfo = raySphereIntersect(rayOrigin, rayDir, atmR);
	if(hitInfo.x > hitInfo.y || hitInfo.y < 0.0) { FragColor = vec4(0.0); return; }

	float tStart = max(0.0, hitInfo.x);
	float tEnd = hitInfo.y;

	// 4. Terrain Occlusion
	if(depthVal < 1.0)
	{
		vec3 worldPos = ReconstructWorldPos(depthVal, uv);
		float viewDistUnit = length(worldPos) / u_realPlanetRadius;
		tEnd = min(tEnd, viewDistUnit);
	}

	if(tStart >= tEnd) { FragColor = vec4(0.0); return; }

	// 5. Raymarch
	float stepSize = (tEnd - tStart) / float(NUM_SAMPLES);
	float halfStep = stepSize * 0.5;

	// Scale Coefficients for Unit Space Integration
	vec3 betaR = u_rayleighScatteringCoeff * u_realPlanetRadius;
	vec3 betaM = u_mieScatteringCoeff * 1.1 * u_realPlanetRadius;
	float H_R = u_rayleighScaleHeight / u_realPlanetRadius;
	float H_M = u_mieScaleHeight / u_realPlanetRadius;

	vec3 totalR = vec3(0.0);
	vec3 totalM = vec3(0.0);
	vec3 opticalDepthR = vec3(0.0);
	vec3 opticalDepthM = vec3(0.0);

	float mu = dot(rayDir, u_sunDirection);
	float pR = RayleighPhase(mu);
	float pM = MiePhase(mu, u_miePreferredScatteringDir);

	float tCurrent = tStart;

	for(int i = 0; i < NUM_SAMPLES; i++)
	{
		vec3 posUnit = rayOrigin + rayDir * (tCurrent + halfStep);
		float r = length(posUnit);
		float h = r - earthR;

		// Density along view ray
		float dR = GetDensity(h, H_R) * stepSize;
		float dM = GetDensity(h, H_M) * stepSize;
		
		opticalDepthR += dR;
		opticalDepthM += dM;

		// Analytic Sphere Shadow Test
		// Simple dot product check for day/night side
		// This avoids shadow map acne completely
		float b = dot(posUnit, u_sunDirection);
		float c = dot(posUnit, posUnit) - earthR * earthR;
		float d = b * b - c;
		
		// Shadow logic: 
		// If d < 0, we miss the sphere (Day). 
		// If d >= 0, we hit sphere line. 
		// If intersection is "forward" (b < 0 check roughly, or exact t check), we are blocked.
		// Exact check: t0 = -b - sqrt(d). If t0 > 0, we are blocked.
		bool blocked = false;
		if(d >= 0.0)
		{
			float t0 = -b - sqrt(d);
			if(t0 > 0.001) blocked = true;
		}

		if(!blocked)
		{
			// Secondary Ray March (Replacing LUT)
			vec2 sunOpticalDepth = GetSunTransmittance(posUnit, u_sunDirection, atmR, earthR, H_R, H_M);
			
			vec3 tau = betaR * (opticalDepthR + sunOpticalDepth.r) + 
					   betaM * (opticalDepthM + sunOpticalDepth.g);
			
			vec3 attn = exp(-tau);
			
			totalR += dR * attn;
			totalM += dM * attn;
		}

		tCurrent += stepSize;
	}

	vec3 color = (totalR * betaR * pR + totalM * betaM * pM) * u_sunIntensity;
	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));

	vec3 ext = exp(-(betaR * opticalDepthR + betaM * opticalDepthM));
	float alpha = 1.0 - clamp((ext.r + ext.g + ext.b) / 3.0, 0.0, 1.0);

	FragColor = vec4(color, alpha);
}
