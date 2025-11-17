#version 330 core
layout(location = 0) in vec2 aPos;

uniform float Scale;
uniform vec2 Offset;
uniform float ScreenRatio;

out vec2 FragPos;

void main()
{
	FragPos = aPos;
	vec2 scaledPos = vec2(aPos.x * Scale, aPos.y * Scale * ScreenRatio);
	gl_Position = vec4(scaledPos + Offset, 1.0, 1.0);
}
