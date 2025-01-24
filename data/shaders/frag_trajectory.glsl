#version 330 core
out vec4 FragColor;
uniform vec3 Color;

in float Alpha;

void main()
{
	if(Alpha != 1.0)
		discard;
	FragColor = vec4(Color, 1.0);
}
