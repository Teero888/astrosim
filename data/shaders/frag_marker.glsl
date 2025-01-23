#version 330 core

out vec4 FragColor;

uniform float Scale;
uniform vec3 Color;

in vec2 FragPos;

void main()
{
	if(FragPos.x < 0.9 && FragPos.x > -0.9 && FragPos.y < 0.9 && FragPos.y > -0.9)
		discard;
	if((FragPos.x < 0.8 && FragPos.x > -0.8) || (FragPos.y < 0.8 && FragPos.y > -0.8))
		discard;
    FragColor = vec4(Color, 1.0);
}

