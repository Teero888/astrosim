#version 330 core

in vec3 FragPos; // Fragment position in world space
in vec3 Normal; // Transformed normal in world space

out vec4 FragColor; // Output color

uniform vec3 lightPos; // Light position in world space
uniform vec3 lightColor; // Light color
uniform vec3 objectColor; // Object color

void main() {
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Combine lighting
    vec3 result = (ambient + diffuse) * objectColor;

    // Output color
    FragColor = vec4(result, 1.0);
}
