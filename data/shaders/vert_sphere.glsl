#version 330 core

layout(location = 0) in vec3 aPos; // Vertex position
layout(location = 1) in vec3 aNormal; // Vertex normal

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float Radius;

out vec3 ScaledFragPos;
out vec3 FragPos;
out vec3 Normal;

void main() {
    FragPos = vec3(model * vec4(aPos * Radius, 1.0));
    ScaledFragPos = vec3(model * vec4(aPos, 1.0));

    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * model * vec4(aPos * Radius, 1.0);
}
