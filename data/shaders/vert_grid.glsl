#version 330 core
layout(location = 0) in vec3 aPos;

uniform float PlanetScale;
uniform float PlanetMass;
uniform float Scale;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 FragWorldPos;
out vec3 FragPos;

void main()
{
	vec3 NewPos = aPos;
	float Distance = length(NewPos);
	NewPos.y = -smoothstep(PlanetMass, 0.0, (Distance * PlanetMass) / PlanetScale) * PlanetScale;

	vec4 WorldPos = vec4(NewPos * Scale, 1.0);
	FragWorldPos = (Model * WorldPos).xyz;
	FragPos = NewPos * Scale;

	gl_Position = Projection * View * WorldPos;
}
