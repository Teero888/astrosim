#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;
uniform float u_logDepthF;

out float Alpha;
out float v_FragDepthW;

void main()
{
	if(aPos == vec3(0.0, 0.0, 0.0))
		Alpha = 0.0;
	else
		Alpha = 1.0;

	gl_Position = Projection * View * Model * vec4(aPos, 1.0);
	v_FragDepthW = gl_Position.w;
}
