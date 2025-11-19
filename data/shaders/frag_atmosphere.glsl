#version 460 core

in vec3 v_rayDirection;
in vec3 v_viewRay;
out vec4 FragColor;

// Texture Inputs
uniform sampler2D u_depthTexture;
uniform sampler2D u_transmittanceLUT;
uniform vec2 u_screenSize;

// Unit Space Inputs
uniform vec3 u_cameraPos;
uniform vec3 u_sunDirection;
uniform float u_atmosphereRadius;

// Physics Input
uniform float u_realPlanetRadius;

// Scattering Params
uniform vec3 u_rayleighScatteringCoeff;
uniform float u_rayleighScaleHeight;
uniform vec3 u_mieScatteringCoeff;
uniform float u_mieScaleHeight;
uniform float u_miePreferredScatteringDir;

// Customizable Intensity
uniform float u_sunIntensity;

// Logarithmic Depth Constant
uniform float u_logDepthF;

// CONFIG
const int NUM_SAMPLES = 64;
const float PI = 3.141592653589793;

// [Optimized] Quadratic Intersection
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

// [Optimized] Boolean Shadow Test
bool testShadow(vec3 ro, vec3 rd, float rad)
{
	float b = dot(ro, rd);
	float c = dot(ro, ro) - rad * rad;
	float d = b * b - c;
	return d >= 0.0 && b <= 0.0;
}

float GetDensity(float h, float H)
{
	return exp(-max(0.0, h) / H);
}

void main()
{
	// Read Depth and Convert to Linear PLANAR Distance (View Z)
	vec2 uv = gl_FragCoord.xy / u_screenSize;
	float logZ = texture(u_depthTexture, uv).r;
	float ndcZ = logZ * 2.0 - 1.0;
	float planarDepth = exp2((ndcZ + 1.0) / u_logDepthF) - 1.0;

	// Convert Planar Depth to Ray Euclidean Distance
	float depthCorrection = length(v_viewRay);
	float linearDist = planarDepth * depthCorrection;

	if(planarDepth > 0.99 * 1e30)
	{
		linearDist = 1e38;
	}

	float sceneDist = linearDist / u_realPlanetRadius;

	// Raymarch Setup
	vec3 rayDir = normalize(v_rayDirection);
	vec3 sunDir = normalize(u_sunDirection);
	float earthR = 1.0;
	float atmR = u_atmosphereRadius;

	vec2 atmosphereIntersect = raySphereIntersect(u_cameraPos, rayDir, atmR);
	if(atmosphereIntersect.y < 0.0)
		discard;

	float tStart = max(0.0, atmosphereIntersect.x);
	float tEnd = atmosphereIntersect.y;

	vec2 planetIntersect = raySphereIntersect(u_cameraPos, rayDir, earthR);
	if(planetIntersect.y > 0.0 && planetIntersect.x > 0.0)
	{
		tEnd = min(tEnd, planetIntersect.x);
	}

	// Occlusion Logic
	tEnd = min(tEnd, sceneDist);

	if(tStart >= tEnd)
	{
		FragColor = vec4(0.0);
		return;
	}

	// Pre-calculate Phase and Coefficients
	float mu = dot(rayDir, sunDir);
	float mu2 = mu * mu;
	float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu2);
	float g = u_miePreferredScatteringDir;
	float g2 = g * g;
	float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu2)) / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * g * mu, 1.5));

	vec3 betaR = u_rayleighScatteringCoeff * u_realPlanetRadius;
	vec3 betaM = u_mieScatteringCoeff * 1.1 * u_realPlanetRadius;

	// 6. Raymarch Loop
	vec3 startPos = u_cameraPos + rayDir * tStart;
	float rayLen = tEnd - tStart;
	float stepSize = rayLen / float(NUM_SAMPLES);
	float halfStep = stepSize * 0.5;

	vec3 totalOpticalDepthR = vec3(0.0);
	vec3 totalOpticalDepthM = vec3(0.0);
	vec3 accumR = vec3(0.0);
	vec3 accumM = vec3(0.0);
	float currentDist = 0.0;

	for(int i = 0; i < NUM_SAMPLES; i++)
	{
		vec3 samplePos = startPos + rayDir * (currentDist + halfStep);
		float r = length(samplePos);
		float height = max(0.0, r - earthR);

		float densityR = GetDensity(height, u_rayleighScaleHeight);
		float densityM = GetDensity(height, u_mieScaleHeight);

		totalOpticalDepthR += densityR * stepSize;
		totalOpticalDepthM += densityM * stepSize;

		// Shadow Check
		if(!testShadow(samplePos, sunDir, earthR))
		{
			vec3 up = samplePos / r;
			float sunCosTheta = dot(up, sunDir);

			// Map CosTheta [-1, 1] to [0, 1]
			float u = sunCosTheta * 0.5 + 0.5;
			// Map Height [0, AtmosphereThickness] to [0, 1]
			float v = height / (atmR - earthR);

			// Clamp to avoid texture wrapping artifacts
			v = clamp(v, 0.0, 1.0);
			u = clamp(u, 0.0, 1.0);

			vec2 opticalDepthToSun = texture(u_transmittanceLUT, vec2(u, v)).rg;

			vec3 tau = betaR * (totalOpticalDepthR + opticalDepthToSun.r) +
				   betaM * (totalOpticalDepthM + opticalDepthToSun.g);

			vec3 attenuation = exp(-tau);

			accumR += densityR * attenuation;
			accumM += densityM * attenuation;
		}
		currentDist += stepSize;
	}

	accumR *= stepSize;
	accumM *= stepSize;

	vec3 inscatter = (accumR * betaR * phaseR + accumM * betaM * phaseM) * u_sunIntensity;

	// Tone Mapping
	vec3 color = inscatter;
	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0 / 2.2));

	// Calculate Alpha based on extinction
	vec3 totalExtinction = exp(-(betaR * totalOpticalDepthR + betaM * totalOpticalDepthM));
	float transmittance = dot(totalExtinction, vec3(1.0)) / 3.0;

	FragColor = vec4(color, 1.0 - transmittance);
}
