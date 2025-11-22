#version 460 core

in vec3 v_rayDirection;
in vec3 v_viewRay;
out vec4 FragColor;

uniform sampler2D u_depthTexture;

// Shadow Map
uniform sampler2D u_shadowMap;
uniform mat4 u_lightSpaceMatrix;
uniform mat4 u_planetRotation;

uniform vec2 u_screenSize;

uniform vec3 u_sunDirection;
uniform vec3 u_cameraPos;
uniform float u_atmosphereRadius;
uniform float u_realPlanetRadius;

// Physics
uniform vec3 u_rayleighScatteringCoeff;
uniform float u_rayleighScaleHeight;
uniform vec3 u_mieScatteringCoeff;
uniform float u_mieScaleHeight;
uniform float u_miePreferredScatteringDir;
uniform float u_sunIntensity;
uniform float u_logDepthF;

const int NUM_SAMPLES = 24;
const int NUM_SUN_SAMPLES = 4;
const float PI = 3.141592653589793;

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

vec3 ReconstructWorldPos(float logZ, vec2 uv)
{
	float viewZ = exp2(logZ / (u_logDepthF * 0.5)) - 1.0;
	vec3 rayDirView = normalize(v_viewRay);
	return rayDirView * (-viewZ / rayDirView.z);
}

float RayleighPhase(float mu) { return 3.0 / (16.0 * PI) * (1.0 + mu * mu); }
float MiePhase(float mu, float g) { return 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5)); }
float GetDensity(float h, float H) { return exp(-max(0.0, h) / H); }

float CalculateShadowOcclusion(vec3 posUnit)
{
	vec3 posLocal = posUnit * u_realPlanetRadius;
	vec3 posWorld = mat3(u_planetRotation) * posLocal;

	// Geometric Bias: Lift sample point out of low-poly noise
	vec3 up = normalize(posLocal);
	vec3 biasedPosWorld = posWorld + (up * 4.0) + u_sunDirection;

	vec4 fragPosLightSpace = u_lightSpaceMatrix * vec4(biasedPosWorld, 1.0);
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	if(projCoords.z > 1.0 || projCoords.x > 1.0 || projCoords.x < 0.0 || projCoords.y > 1.0 || projCoords.y < 0.0)
		return 0.0;

	float shadowMapDepth = texture(u_shadowMap, projCoords.xy).r;
	float currentDepth = projCoords.z;

	float bias = 0.0005;
	return (currentDepth - bias > shadowMapDepth) ? 1.0 : 0.0;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / u_screenSize;
	float depthVal = texelFetch(u_depthTexture, ivec2(gl_FragCoord.xy), 0).r;

	float earthR = 1.0;
	float atmR = u_atmosphereRadius / u_realPlanetRadius;
	vec3 rayOrigin = u_cameraPos;
	vec3 rayDir = normalize(v_rayDirection);

	vec2 hitInfo = raySphereIntersect(rayOrigin, rayDir, atmR);
	if(hitInfo.x > hitInfo.y || hitInfo.y < 0.0)
	{
		FragColor = vec4(0.0);
		return;
	}

	float tStart = max(0.0, hitInfo.x);
	float tEnd = hitInfo.y;

	if(depthVal < 1.0)
	{
		vec3 worldPos = ReconstructWorldPos(depthVal, uv);
		float viewDistUnit = length(worldPos) / u_realPlanetRadius;
		tEnd = min(tEnd, viewDistUnit);
	}

/* 	vec2 planetHit = raySphereIntersect(rayOrigin, rayDir, earthR);
	if(planetHit.x < 1000.0 && planetHit.x > 0.0)
	{
		tEnd = min(tEnd, planetHit.x);
	} */

	if(tStart >= tEnd)
	{
		FragColor = vec4(0.0);
		return;
	}

	float stepSize = (tEnd - tStart) / float(NUM_SAMPLES);
	float halfStep = stepSize * 0.5;

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

		float dR = GetDensity(h, H_R) * stepSize;
		float dM = GetDensity(h, H_M) * stepSize;

		opticalDepthR += dR;
		opticalDepthM += dM;

		float occlusion = CalculateShadowOcclusion(posUnit);
		float shadowFactor = 1.0 - occlusion;

		if(shadowFactor > 0.0)
		{
			float sunStep = (raySphereIntersect(posUnit, u_sunDirection, atmR).y) / float(NUM_SUN_SAMPLES);
			float sunHalf = sunStep * 0.5;
			float sunOptR = 0.0;
			float sunOptM = 0.0;

			for(int j = 0; j < NUM_SUN_SAMPLES; j++)
			{
				vec3 posSun = posUnit + u_sunDirection * (float(j) * sunStep + sunHalf);
				float hSun = length(posSun) - earthR;
				sunOptR += GetDensity(hSun, H_R);
				sunOptM += GetDensity(hSun, H_M);
			}

			vec3 tau = betaR * (opticalDepthR + sunOptR * sunStep) +
				   betaM * (opticalDepthM + sunOptM * sunStep);

			vec3 attn = exp(-tau);

			totalR += dR * attn * shadowFactor;
			totalM += dM * attn * shadowFactor;
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
