#version 330 core

in vec3 FragPos; // fragment position in world space
in vec3 Normal; // transformed normal in world space

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform bool uSource;

void main()
{
	if(uSource)
	{
		FragColor = vec4(uObjectColor, 1.0);
		return;
	}
	// // ambient lighting
	// float ambientStrength = 0.1;
	// vec3 ambient = ambientStrength * lightColor;

	// diffuse lighting
	vec3 norm = normalize(Normal);
	float diff = max(dot(norm, uLightDir), 0.0);
	vec3 diffuse = diff * uLightColor;

	// combine lighting
	vec3 result = diffuse * uObjectColor;

	FragColor = vec4(result, 1.0);
}
