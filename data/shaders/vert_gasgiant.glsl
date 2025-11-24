#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uInvModel;

out vec3 vLocalPos; // Position in Unit Cube Space (-1 to 1)
out vec3 vRayOrigin; // Camera Position in Unit Cube Space

void main()
{
	// Standard Transform
	vec4 worldPos = uModel * vec4(aPos, 1.0);
	gl_Position = uProjection * uView * worldPos;

	// Pass Local Position
	vLocalPos = aPos;

	vec4 camPosLocal = uInvModel * vec4(0.0, 0.0, 0.0, 1.0);
	vRayOrigin = camPosLocal.xyz / camPosLocal.w;
}
