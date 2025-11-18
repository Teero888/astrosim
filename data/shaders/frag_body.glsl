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

// == TERRESTRIAL ==
uniform float uMountainStartMin;
uniform float uOceanDepthMax;
uniform float uBeachHeightMax;
uniform float uLandHeightMax;
uniform float uMountainHeightMax;
uniform float uSnowLineStart;
uniform float uSnowLineEnd;

// == VOLCANIC ==
uniform float uLavaPoolHeight;
uniform float uLavaRockHeight;
uniform float uLavaFlowStart;
uniform float uLavaFlowMaskEnd;
uniform float uLavaPeakHeight;
uniform float uLavaHotspotHeight;

// == ICE ==
uniform float uSlushDepthMax;
uniform float uIceSheetHeight;
uniform float uCrevasseStart;
uniform float uCrevasseMaskEnd;
uniform float uCrevasseColorMix;

// == BARREN ==
uniform float uBarrenLandMax;
uniform float uBarrenMountainMax;

vec3 GetTerrainColor()
{
	float elevation = vColorData.x;
	float special_noise = vColorData.y; // mountain_noise, lava_noise, crevasse_noise...

	// Terrestrial
	if(uTerrainType == 0)
	{
		float mountain_noise_component = special_noise;
		if(elevation < 0.0) // Water
		{
			float depth_ratio = clamp(-elevation / uOceanDepthMax, 0.0, 1.0);
			return mix(SHALLOW_OCEAN_COLOR, DEEP_OCEAN_COLOR, depth_ratio);
		}
		else // Land
		{
			// Beach from 0m to uBeachHeightMax (e.g., 50m)
			if(elevation < uBeachHeightMax)
			{
				return mix(LAND_LOW_COLOR, BEACH_COLOR, elevation / uBeachHeightMax);
			}

			// Check if it's a mountain or a hill based on the mountain noise component
			if(mountain_noise_component > uMountainStartMin)
			{
				// Mountain color blends based on *total* elevation
				float mountain_ratio = clamp(elevation / uMountainHeightMax, 0.0, 1.0);
				vec3 mountain_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, mountain_ratio);

				// Snow line starts at uSnowLineStart
				if(elevation > uSnowLineStart)
				{
					// Snow fades in from uSnowLineStart to uSnowLineEnd
					float snow_ratio = clamp((elevation - uSnowLineStart) / (uSnowLineEnd - uSnowLineStart), 0.0, 1.0);
					return mix(mountain_color, SNOW_COLOR, snow_ratio);
				}
				return mountain_color;
			}
			else // Lowlands/hills
			{
				// Land blends from uBeachHeightMax to uLandHeightMax
				float land_ratio = clamp((elevation - uBeachHeightMax) / (uLandHeightMax - uBeachHeightMax), 0.0, 1.0);
				return mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);
			}
		}
	}
	// Volcanic
	else if(uTerrainType == 1)
	{
		float lava_noise = special_noise; // [0, 1] raw noise
		// Use "ocean" for lava pools (low elevation)
		if(elevation < uLavaPoolHeight)
		{
			// blend from dark rock to bright lava
			float lava_ratio = clamp(elevation / uLavaPoolHeight, 0.0, 1.0);
			return mix(DEEP_OCEAN_COLOR, SHALLOW_OCEAN_COLOR, lava_ratio);
		}

		// Use "land" for cooled rock
		float land_ratio = clamp(elevation / uLavaRockHeight, 0.0, 1.0);
		vec3 rock_color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Use "mountain" for active lava flows
		// Use lava_noise as a mask for glowing lava
		if(lava_noise > uLavaFlowStart)
		{
			float flow_mask = (lava_noise - uLavaFlowStart) / (uLavaFlowMaskEnd - uLavaFlowStart);
			flow_mask = clamp(flow_mask, 0.0, 1.0);

			// Use mountain colors for lava
			float mountain_ratio = clamp(elevation / uLavaPeakHeight, 0.0, 1.0);
			vec3 lava_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, mountain_ratio);

			// Add snow color as "brightest hot"
			float hotspot_ratio = clamp((elevation - uLavaPeakHeight) / (uLavaHotspotHeight - uLavaPeakHeight), 0.0, 1.0);
			lava_color = mix(lava_color, SNOW_COLOR, hotspot_ratio * flow_mask);

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
			float depth_ratio = clamp(-elevation / uSlushDepthMax, 0.0, 1.0);
			return mix(SHALLOW_OCEAN_COLOR, DEEP_OCEAN_COLOR, depth_ratio);
		}

		// Use "land" colors for basic ice sheet
		float land_ratio = clamp(elevation / uIceSheetHeight, 0.0, 1.0);
		vec3 ice_color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Use "mountain" colors for deep crevasse shadows
		if(crevasse_noise > uCrevasseStart)
		{
			float crevasse_mask = (crevasse_noise - uCrevasseStart) / (uCrevasseMaskEnd - uCrevasseStart);
			crevasse_mask = clamp(crevasse_mask, 0.0, 1.0);
			vec3 crevasse_color = mix(MOUNTAIN_LOW_COLOR, MOUNTAIN_HIGH_COLOR, crevasse_mask);
			return mix(ice_color, crevasse_color, crevasse_mask * uCrevasseColorMix);
		}
		return ice_color;
	}
	// Barren
	else if(uTerrainType == 3)
	{
		float mountain_raw_noise = special_noise; // [0, 1]
		// No water, just blend between rock colors based on elevation
		float land_ratio = clamp(elevation / uBarrenLandMax, 0.0, 1.0);
		vec3 color = mix(LAND_LOW_COLOR, LAND_HIGH_COLOR, land_ratio);

		// Add "mountain" colors based on noise, not just elevation
		float mountain_ratio = clamp(elevation / uBarrenMountainMax, 0.0, 1.0);
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
