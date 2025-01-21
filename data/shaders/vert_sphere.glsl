#version 330 core

layout(location = 0) in vec3 aPos; // Vertex position
layout(location = 1) in vec3 aNormal; // Vertex normal

uniform mat4 model; // Model matrix (transforms object to world space)
uniform mat4 view; // View matrix (transforms world to camera space)
uniform mat4 projection; // Projection matrix (transforms camera to clip space)

out vec3 FragPos; // Fragment position in world space
out vec3 Normal; // Transformed normal in world space

void main() {
    // Transform vertex position to world space
    FragPos = vec3(model * vec4(aPos, 1.0));

    // Transform normal to world space (correct for non-uniform scaling)
    Normal = mat3(transpose(inverse(model))) * aNormal;

    // Transform vertex position to clip space
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
