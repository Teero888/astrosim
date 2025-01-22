#version 330 core
layout(location = 0) in vec3 aPos; // Vertex position in object space

uniform float Scale;
uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 FragWorldPos;   // World position of the fragment
out vec3 FragPos;   // World position of the fragment

void main()
{
    // Transform the vertex position to world space
    vec4 WorldPos = vec4(aPos * Scale, 1.0);
    FragWorldPos = (Model * WorldPos).xyz;
	FragPos = aPos * Scale;

    // Transform the vertex position to clip space
    gl_Position = Projection * View * WorldPos;
}
