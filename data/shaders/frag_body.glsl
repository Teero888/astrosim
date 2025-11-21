#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec4 vColorData;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform vec3 uViewPos;
uniform float uAmbientStrength;
uniform float uSpecularStrength;
uniform float uShininess;
uniform bool uSource;
uniform int uTerrainType;

// Logarithmic Depth
uniform float u_logDepthF;
uniform bool uIsShadowPass;

// Shadows
uniform sampler2D u_shadowMap;
uniform mat4 u_lightSpaceMatrix;

// Palette Uniforms
uniform vec3 uDeepOcean;
uniform vec3 uShallowOcean;
uniform vec3 uBeach;
uniform vec3 uGrass;
uniform vec3 uForest;
uniform vec3 uDesert;
uniform vec3 uSnow;
uniform vec3 uRock;
uniform vec3 uTundra;

float LinearizeDepth(float depth)
{
	float z = depth * 2.0 - 1.0;
	return (2.0 * 0.1 * 1e30) / (1e30 + 0.1 - z * (1e30 - 0.1));
}

// Calculate Shadow with PCF (Percentage-Closer Filtering) and Slope-Scale Bias
float CalculateShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
	vec4 fragPosLightSpace = u_lightSpaceMatrix * vec4(fragPos, 1.0);

	// Perform perspective divide
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	// Transform to [0,1] range
	projCoords = projCoords * 0.5 + 0.5;

	// Keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
	if(projCoords.z > 1.0)
		return 0.0;

	// Calculate Bias based on surface slope to prevent acne
	// On 1:1 scale terrain, this needs to be adaptive
	float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.00005);

	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0);

	// 3x3 PCF Sampling
	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(u_shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
			// Check if current fragment is behind the shadow map depth
			// Note: In our log-depth shadow map, we compare raw values directly
			shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
		}
	}
	shadow /= 9.0;

	return shadow;
}

vec3 GetBiomeColor(float temp, float moisture)
{
	vec3 biomeColor;
	float t_weight = smoothstep(0.2, 0.7, temp);
	float m_weight = smoothstep(0.2, 0.8, moisture);

	vec3 coldColor = mix(uTundra, uSnow, m_weight);
	vec3 hotDry = mix(uDesert, uGrass, smoothstep(0.1, 0.45, moisture));
	vec3 hotColor = mix(hotDry, uForest, smoothstep(0.55, 0.9, moisture));
	return mix(coldColor, hotColor, t_weight);
}

vec3 GetTerrainColor()
{
	float elevation = vColorData.x;
	float temp = vColorData.y;
	float moisture = vColorData.z;
	float ice_mask = vColorData.w;

	if(uTerrainType == 3)
		return mix(uDesert, uRock, smoothstep(0.0, 0.8, elevation / 3000.0));

	if(elevation < 0.0)
	{
		float depth = clamp(-elevation / 2000.0, 0.0, 1.0);
		return mix(uShallowOcean, uDeepOcean, depth);
	}

	vec3 up = normalize(FragPos + uViewPos);
	float slope = 1.0 - dot(normalize(Normal), up);

	vec3 terrainColor = GetBiomeColor(temp, moisture);
	float beach_alpha = 1.0 - smoothstep(10.0, 60.0, elevation);
	terrainColor = mix(terrainColor, uBeach, beach_alpha);
	float rock_factor = smoothstep(0.20, 0.45, slope);
	terrainColor = mix(terrainColor, uRock, rock_factor);

	if(ice_mask > 0.01)
	{
		float ice_alpha = smoothstep(0.0, 0.5, ice_mask);
		terrainColor = mix(terrainColor, uSnow, ice_alpha);
	}
	return terrainColor;
}

void main()
{
	// == CRITICAL FIX: UNIFIED DEPTH WRITING ==
	// Both Shadow Pass and Main Pass MUST write the exact same logarithmic depth value.
	float linear_z = gl_FragCoord.w; // 1/w
	if(isinf(linear_z))
		linear_z = 1e30; // Safety

	// Standard Logarithmic Depth Calculation
	// C = 1.0.  F is computed on CPU as 2.0 / log2(far + 1.0)
	// Result is in [-1, 1] (NDC z), then converted to [0, 1] for gl_FragDepth automatically
	// However, manually writing gl_FragDepth writes the [0,1] value directly.

	// We use the derivation: depth = log2(C * w + 1) * F * 0.5
	// Note: FragPos is Camera Relative. -FragPos.z is roughly w.
	// gl_FragCoord.w is 1/w. So w = 1/gl_FragCoord.w.

	float w = 1.0 / gl_FragCoord.w;
	float log_z = log2(w + 1.0) * u_logDepthF * 0.5;
	gl_FragDepth = log_z;

	if(uIsShadowPass)
	{
		// In shadow pass, we only care about the depth.
		// Discard "transparent" fragments if you had alpha testing,
		// but for terrain we just return.
		FragColor = vec4(1.0);
		return;
	}

	if(uSource)
	{
		FragColor = vec4(uObjectColor, 1.0);
		return;
	}

	vec3 terrain_color = GetTerrainColor();
	float ice_mask = vColorData.w;

	float specMod = 1.0;
	if(vColorData.x < 0.0)
		specMod = 3.0;
	else if(ice_mask > 0.5)
		specMod = 2.0;
	else
		specMod = 0.01;

	vec3 ambient = uAmbientStrength * uLightColor;
	vec3 norm = normalize(Normal);
	vec3 lightDir = normalize(uLightDir);

	float NdotL = max(dot(norm, lightDir), 0.0);

	// Calculate Shadow
	float shadow = CalculateShadow(FragPos, norm, lightDir);

	vec3 diffuse = NdotL * uLightColor;

	vec3 viewDir = normalize(-FragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(norm, halfwayDir), 0.0), uShininess);
	vec3 specular = uSpecularStrength * specMod * spec * uLightColor;

	if(NdotL <= 0.0)
		specular = vec3(0.0);

	// Apply Shadow (0.0 = fully shadowed)
	// We keep ambient light even in shadow
	vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular));

	vec3 finalColor = lighting * terrain_color;

	// Tone Mapping (Reinhard)
	finalColor = finalColor / (finalColor + vec3(1.0));
	// Gamma Correction
	finalColor = pow(finalColor, vec3(1.0 / 2.2));

	FragColor = vec4(finalColor, 1.0);
}
