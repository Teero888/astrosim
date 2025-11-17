#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float u_logDepthF;

out vec3 FragPos;
out vec3 Normal;

void main()
{
	vec4 worldPos = uModel * vec4(aPos, 1.0);
	FragPos = vec3(worldPos);
	Normal = mat3(transpose(inverse(uModel))) * aNormal;
	gl_Position = uProjection * uView * worldPos;

	// Logarithmic depth buffer
	gl_Position.z = (log2(max(1e-6, gl_Position.w) + 1.0)) * u_logDepthF - 1.0;
	gl_Position.z *= gl_Position.w;
}
