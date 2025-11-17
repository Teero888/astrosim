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
uniform int uTerrainType;

uniform vec3 DEEP_OCEAN_COLOR;
uniform vec3 SHALLOW_OCEAN_COLOR;
uniform vec3 BEACH_COLOR;
uniform vec3 LAND_LOW_COLOR;
uniform vec3 LAND_HIGH_COLOR;
uniform vec3 MOUNTAIN_LOW_COLOR;
uniform vec3 MOUNTAIN_HIGH_COLOR;
uniform vec3 SNOW_COLOR;

vec3 GetTerrainColor()
{
	float elevation = vColorData.x;
	float special_noise = vColorData.y; // mountain_noise, lava_noise, crevasse_noise...

	// Terrestrial
	if(uTerrainType == 0)
	{
		float mountain_noise = special_noise;
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
	// Volcanic
	else if(uTerrainType == 1)
	{
		float lava_noise = special_noise; // [0, 1] raw noise
		// Use "ocean" for lava pools (low elevation)
		if(elevation < uPlanetRadius * 0.001)
		{
			// blend from dark rock to bright lava
			float lava_ratio = clamp(elevation / (uPlanetRadius * 0.001), 0.0, 1.0);
			return mix(DEEP_OCEAN_COLOR, SHALLOW_OCEAN_COLOR, lava_ratio);
		}

		// Use "land" for cooled rock
		float land_ratio = clamp(elevation / (uPlanetRadius * 0.01), 0.0, 1.0);
		vec3 rock_color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Use "mountain" for active lava flows
		// Use lava_noise as a mask for glowing lava
		if(lava_noise > 0.6)
		{
			float flow_mask = (lava_noise - 0.6) / 0.4;
			// Use mountain colors for lava
			float mountain_ratio = clamp(elevation / (uPlanetRadius * 0.02), 0.0, 1.0);
			vec3 lava_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, mountain_ratio);
			// Add snow color as "brightest hot"
			lava_color = mix(lava_color, SNOW_COLOR, mountain_ratio * flow_mask);

			return mix(rock_color, lava_color, flow_mask * 0.8); // 80% lava
		}
		return rock_color;
	}
	// Ice
	else if(uTerrainType == 2)
	{
		float crevasse_noise = special_noise; // [0, 1] crevasse strength
		// Use "ocean" colors for water/slush at low points
		if(elevation < 0.0)
		{
			float depth_ratio = clamp(-elevation / (uPlanetRadius * 0.01), 0.0, 1.0);
			return mix(SHALLOW_OCEAN_COLOR, DEEP_OCEAN_COLOR, depth_ratio);
		}

		// Use "land" colors for basic ice sheet
		float land_ratio = clamp(elevation / (uPlanetRadius * 0.001), 0.0, 1.0);
		vec3 ice_color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Use "mountain" colors for deep crevasse shadows
		if(crevasse_noise > 0.1)
		{
			vec3 crevasse_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, crevasse_noise);
			return mix(ice_color, crevasse_color, crevasse_noise * 0.7);
		}
		return ice_color;
	}
	// Barren
	else if(uTerrainType == 3)
	{
		float mountain_raw_noise = special_noise; // [0, 1]
		// No water, just blend between rock colors based on elevation
		float land_ratio = clamp(elevation / (uPlanetRadius * 0.01), 0.0, 1.0);
		vec3 color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Add "mountain" colors based on noise, not just elevation
		float mountain_ratio = clamp(elevation / (uPlanetRadius * 0.015), 0.0, 1.0);
		vec3 mountain_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, mountain_ratio);

		return mix(color, mountain_color, mountain_raw_noise * land_ratio);
	}

	// Fallback
	return vec3(1.0, 0.0, 1.0); // Pink error
}

void main()
{
	if(uSource)
	{
		FragColor = vec4(uObjectColor, 1.0);
		return;
	}

	vec3 terrain_color = GetTerrainColor();

	// diffuse lighting
	vec3 norm = normalize(Normal);
	float diff = max(dot(norm, uLightDir), 0.0);
	vec3 diffuse = diff * uLightColor;

	// combine lighting
	vec3 result = diffuse * terrain_color;

	FragColor = vec4(result, 1.0);
}
