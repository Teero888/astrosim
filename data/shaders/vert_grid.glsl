#version 330 core
layout(location = 0) in vec3 aPos;

uniform float Scale;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 FragWorldPos;
out vec3 FragPos;

void main()
{
    vec4 WorldPos = vec4(aPos * Scale, 1.0);
    FragWorldPos = (Model * WorldPos).xyz;
	FragPos = aPos * Scale;

    // transform the vertex position to clip space
    gl_Position = Projection * View * WorldPos;
}
