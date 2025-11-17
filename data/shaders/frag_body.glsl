#version 330 core

in vec3 FragPos; // fragment position in world space
in vec3 Normal; // transformed normal in world space
in vec2 vColorData;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform float uPlanetRadius;
uniform bool uSource;

uniform vec3 DEEP_OCEAN_COLOR;
uniform vec3 SHALLOW_OCEAN_COLOR;
uniform vec3 BEACH_COLOR;
uniform vec3 LAND_LOW_COLOR;
uniform vec3 LAND_HIGH_COLOR;
uniform vec3 MOUNTAIN_LOW_COLOR;
uniform vec3 MOUNTAIN_HIGH_COLOR;
uniform vec3 SNOW_COLOR;

vec3 getTerrainColor()
{
	float elevation = vColorData.x;
	float mountain_noise = vColorData.y;

	if(elevation < 0.0) // Water
	{
		float depth_ratio = clamp(-elevation / (uPlanetRadius * 0.01), 0.0, 1.0);
		return mix(SHALLOW_OCEAN_COLOR, DEEP_OCEAN_COLOR, depth_ratio);
	}
	else // Land
	{
		float beach_height = uPlanetRadius * 0.0005;
		if(elevation < beach_height)
		{
			return mix(LAND_LOW_COLOR, BEACH_COLOR, elevation / beach_height);
		}

		if(mountain_noise > uPlanetRadius * 0.001) // Mountainous areas
		{
			float mountain_ratio = clamp((elevation - uPlanetRadius * 0.001) / (uPlanetRadius * 0.02), 0.0, 1.0);
			vec3 mountain_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, mountain_ratio);
			if(elevation > uPlanetRadius * 0.01) // High peaks
			{
				float snow_ratio = clamp((elevation - uPlanetRadius * 0.01) / (uPlanetRadius * 0.01), 0.0, 1.0);
				return mix(mountain_color, SNOW_COLOR, snow_ratio);
			}
			return mountain_color;
		}
		else // Lowlands/hills
		{
			float land_ratio = clamp(elevation / (uPlanetRadius * 0.005), 0.0, 1.0);
			return mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);
		}
	}
}

void main()
{
	if(uSource)
	{
		FragColor = vec4(uObjectColor, 1.0);
		return;
	}

	vec3 terrainColor = getTerrainColor();

	// diffuse lighting
	vec3 norm = normalize(Normal);
	float diff = max(dot(norm, uLightDir), 0.0);
	vec3 diffuse = diff * uLightColor;

	// combine lighting
	vec3 result = diffuse * terrainColor;

	FragColor = vec4(result, 1.0);
}
