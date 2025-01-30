#version 460 core
layout(location = 0) in vec3 aPos;

uniform float Scale;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

// Define a struct for planet data
// struct PlanetInfo {
// 	float Scale;
// 	float Mass;
// 	vec3 Position;
// };

// Declare an SSBO for the planet array
// layout(std430, binding = 0) buffer PlanetData {
// 	PlanetInfo Planets[];
// };

out vec3 FragWorldPos;
out vec3 FragPos;

void main()
{
	vec3 NewPos = aPos;

	vec4 WorldPos = vec4(NewPos * Scale, 1.0);
	FragWorldPos = (Model * WorldPos).xyz;
	FragPos = NewPos * Scale;

	// for (int i = 0; i < Planets.length(); i++) {
	// 	float PlanetScale = Planets[i].Scale;
	// 	float PlanetMass = Planets[i].Mass;
	// 	vec3 PlanetPosition = Planets[i].Position;
	//
	// 	float DistanceToPlanet = length(WorldPos - PlanetPosition);
	//
	// 	NewPos.y -= smoothstep(PlanetMass, 0.0, (DistanceToPlanet * PlanetMass) / PlanetScale) * PlanetScale;
	// 	break;
	// }
	//
	// WorldPos = vec4(NewPos * Scale, 1.0);
	// FragWorldPos = (Model * WorldPos).xyz;
	// FragPos = NewPos * Scale;
	gl_Position = Projection * View * WorldPos;
}
