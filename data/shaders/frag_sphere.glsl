// --------------------- FRAGMENT SHADER ---------------------
#version 330 core

in vec3 ViewDir;
in vec3 WorldPos;
in vec3 Normal;
in vec3 LocalPos;
in vec3 TangentLightDir;

out vec4 FragColor;

// Celestial Body Parameters
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float radius;
uniform float albedo;
uniform float roughness;
uniform float specularity;
uniform vec3 surfaceColor;
uniform vec3 oceanColor;
uniform vec3 cloudColor;
uniform float textureScale;
uniform vec3 atmoColor;
uniform float atmoStrength;
uniform float cloudCover;
uniform float time;
uniform float coreTemperature;
uniform float surfaceTemperature;
uniform float orbitalSpeed;
uniform float rotationSpeed;
uniform float magneticField;
uniform float auroraIntensity;

const float PI = 3.14159265359;

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

float fbm(vec4 p, int octaves)
{
	float value = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;

	for(int i = 0; i < octaves; i++)
	{
		value += amplitude * noise(p * frequency);
		frequency *= 2.0;
		amplitude *= 0.5;
		p += vec4(12.345, 67.890, 123.456, 789.012);
	}
	return value;
}

vec3 calculateSurface(vec3 pos, vec3 normal)
{
    // Multi-layer procedural surface
    vec4 coord = vec4(pos * textureScale, time * rotationSpeed * 0.01);

    // Terrain elevation
    float height = fbm(coord * 2.0, 5) * 2.0;

    // Ocean mask with a sharper transition
    float oceanMask = smoothstep(-0.0, -0.08, height); // Narrower range

    // Base color
    vec3 color = mix(oceanColor, surfaceColor, oceanMask);

    // Add temperature-based coloration
    float tempVariation = 0.5 + 0.5 * sin(pos.y * 10.0 + time);
    color *= mix(0.8, 1.2, tempVariation);

    return color;
}

vec3 calculateClouds(vec3 pos, vec3 normal)
{
    // Cloud coordinates and FBM noise
    vec4 cloudCoord = vec4(pos * textureScale * 0.6, time * 0.1);
    float clouds = fbm(cloudCoord * 4.0, 4);
    clouds = smoothstep(0.5 - cloudCover, 0.6 - cloudCover, clouds);

    // Make clouds more translucent on mountains
	float terrainHeight = length(WorldPos) - radius;
    float heightFactor = smoothstep(0.5, 0.8, terrainHeight); // Adjust these values as needed
    clouds *= 1.0 - heightFactor * 0.8; // Reduce opacity in mountainous regions

    // Cloud lighting
    float cloudDiffuse = max(dot(normal, normalize(lightPos - WorldPos)), 0.0);
    return vec3(clouds * (0.8 + 0.2 * cloudDiffuse));
}

vec3 calculateAtmosphere(vec3 normal)
{
	float rim = 1.0 - max(dot(ViewDir, normal), 0.0);
	float depth = length(WorldPos) - radius;

	// Rayleigh scattering approximation
	vec3 scattering = atmoColor * pow(rim, 3.0) * (1.0 + depth * 0.1);

	// Mie scattering
	float mie = pow(rim, 0.5) * atmoStrength * 4.0;

	return scattering * (mie + 0.5) * atmoStrength * 4.0;
}

vec3 calculateAurora(vec3 pos)
{
	if(auroraIntensity < 0.01)
		return vec3(0);

	// Polar regions only
	float latitude = acos(pos.y) * (1.0 / PI);
	if(abs(latitude) < 0.4)
		return vec3(0);

	vec4 auroraCoord = vec4(pos * 2.0, time * 0.05);
	float aurora = fbm(auroraCoord * 10.0, 3);
	aurora = smoothstep(0.6, 0.8, aurora) * auroraIntensity;

	vec3 color = mix(vec3(0.1, 0.8, 0.2), vec3(0.8, 0.2, 0.5),
		smoothstep(0.3, 0.7, fract(time * 0.1)));

	return aurora * color * magneticField;
}

vec3 calculateLighting(vec3 normal, vec3 surface, vec3 clouds)
{
	// PBR-inspired lighting
	vec3 lightDir = normalize(lightPos - WorldPos);
	vec3 halfwayDir = normalize(lightDir + ViewDir);

	// Diffuse
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = NdotL * lightColor * albedo * surface;

	// Specular (GGX approximation)
	float alpha = roughness * roughness;
	float NdotH = max(dot(normal, halfwayDir), 0.0);
	float denom = NdotH * NdotH * (alpha - 1.0) + 1.0;
	float spec = alpha / (PI * denom * denom);

	// Fresnel (Schlick approx)
	float fresnel = pow(1.0 - max(dot(ViewDir, halfwayDir), 0.0), 5.0);

	vec3 specColor = mix(vec3(0.04), surface, specularity);
	vec3 specular = spec * fresnel * specColor * lightColor * NdotL;

	// Combine with clouds
	vec3 final = diffuse + specular;
	final = mix(final, cloudColor, clouds.r);

	// Emissive (for stars)
	// float coreGlow = smoothstep(5000.0, 15.0e6, coreTemperature);
	// vec3 emissive = surface * coreGlow * exp2(coreTemperature * 0.0001);

	return final + surface * (CloudColor * clouds.r);
}

void main()
{
	vec3 normal = normalize(Normal);

	// Surface calculation
	vec3 surface = calculateSurface(LocalPos, normal);
	vec3 clouds = calculateClouds(LocalPos, normal);

	// Atmospheric effects
	vec3 atmosphere = calculateAtmosphere(normal);
	vec3 aurora = calculateAurora(LocalPos);

	// Final lighting
	vec3 litSurface = calculateLighting(normal, surface, clouds);

	// Combine all elements
	vec3 finalColor = litSurface + atmosphere + aurora;

	// Debugging
	// surface
	// vec4 coord = vec4(LocalPos * textureScale, time * rotationSpeed * 0.01);
	// float height = fbm(coord * 2.0, 5);
	// // FragColor = vec4(vec3(height), 1.0);
	//
	// // Ocean mask
	// // float oceanMask = smoothstep(0.0, 1.0, height);

	// Gamma correction
	finalColor = pow(finalColor, vec3(1.0 / 2.2));

	FragColor = vec4(finalColor, 1.0);
}
