#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;
uniform float u_logDepthF;

out float Alpha;

void main()
{
	if(aPos == vec3(0.0, 0.0, 0.0))
		Alpha = 0.0;
	else
		Alpha = 1.0;
	gl_Position = Projection * View * Model * vec4(aPos, 1.0);

	// Logarithmic depth buffer
	gl_Position.z = (log2(max(1e-6, gl_Position.w) + 1.0)) * u_logDepthF - 1.0;
	gl_Position.z *= gl_Position.w;
}
