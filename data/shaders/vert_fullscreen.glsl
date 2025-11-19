#version 460 core
layout(location = 0) in vec2 aPos; // Vertices for a -1 to 1 quad

void main()
{
	gl_Position = vec4(aPos, 0.0, 1.0);
}
