#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 3) in vec4 aColorData;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 FragPos;
out vec3 Normal;
out vec4 vColorData;
// out float v_view_z; // Removed

void main()
{
	vec4 worldPos = uModel * vec4(aPos, 1.0);
	FragPos = vec3(worldPos);
	Normal = mat3(transpose(inverse(uModel))) * aNormal;
	vColorData = aColorData;

	gl_Position = uProjection * uView * worldPos;
}
