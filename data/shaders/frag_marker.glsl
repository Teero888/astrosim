#version 330 core

out vec4 FragColor;

uniform float Scale;
uniform vec3 Color;

in vec2 FragPos;

void main()
{
	// Discard the inner part of the quad to draw a border.
	// The quad is a 2x2 square, from -1 to 1.
	if(FragPos.x < 0.9 && FragPos.x > -0.9 && FragPos.y < 0.9 && FragPos.y > -0.9)
		discard;
	if((FragPos.x < 0.8 && FragPos.x > -0.8) || (FragPos.y < 0.8 && FragPos.y > -0.8))
		discard;
	FragColor = vec4(Color, 1.0);
}
