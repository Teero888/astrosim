#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec4 vColorData;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform vec3 uViewPos;
uniform vec3 uPlanetCenterRelCam;
uniform float uAmbientStrength;
uniform float uSpecularStrength;
uniform float uShininess;
uniform bool uSource;
uniform int uTerrainType;

uniform float u_logDepthF;
uniform bool uIsShadowPass;

uniform sampler2D u_shadowMap;
uniform mat4 u_lightSpaceMatrix;

uniform vec3 uDeepOcean;
uniform vec3 uShallowOcean;
uniform vec3 uBeach;
uniform vec3 uGrass;
uniform vec3 uForest;
uniform vec3 uDesert;
uniform vec3 uSnow;
uniform vec3 uRock;
uniform vec3 uTundra;

float CalculateShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
	// Apply normal bias to prevent surface acne
	float normalBias = 0.5;
	vec3 biasedPos = fragPos + normal * normalBias;

	vec4 fragPosLightSpace = u_lightSpaceMatrix * vec4(biasedPos, 1.0);

	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;

	if(projCoords.z > 1.0)
		return 0.0;

	float bias = 0.0005;
	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0);

	for(int x = -1; x <= 1; ++x)
	{
		for(int y = -1; y <= 1; ++y)
		{
			float pcfDepth = texture(u_shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
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

	vec3 up = normalize(FragPos - uPlanetCenterRelCam);
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
	if(uIsShadowPass)
	{
		FragColor = vec4(1.0);
		return;
	}

	float w = 1.0 / gl_FragCoord.w;
	float log_z = log2(w + 1.0) * u_logDepthF * 0.5;
	gl_FragDepth = log_z;

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

	float shadow = CalculateShadow(FragPos, norm, lightDir);

	vec3 diffuse = NdotL * uLightColor;

	vec3 viewDir = normalize(-FragPos);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(norm, halfwayDir), 0.0), uShininess);
	vec3 specular = uSpecularStrength * specMod * spec * uLightColor;

	if(NdotL <= 0.0)
		specular = vec3(0.0);

	vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular));

	vec3 finalColor = lighting * terrain_color;

	finalColor = finalColor / (finalColor + vec3(1.0));
	finalColor = pow(finalColor, vec3(1.0 / 2.2));

	FragColor = vec4(finalColor, 1.0);
}
