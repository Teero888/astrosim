#version 330 core
out vec4 FragColor;
uniform vec3 Color;
uniform float u_logDepthF;

in float Alpha;
in float v_FragDepthW;

void main()
{
	if(Alpha != 1.0)
		discard;
	float log_z = log2(v_FragDepthW + 1.0) * u_logDepthF * 0.5;
	gl_FragDepth = log_z;

	FragColor = vec4(Color, 1.0);
}
