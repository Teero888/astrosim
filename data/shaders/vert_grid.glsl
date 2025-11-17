#version 460 core
layout(location = 0) in vec2 aPos; // Vertices for a -1 to 1 quad

uniform mat4 u_invView;
uniform mat4 u_invProjection;

out vec3 v_rayDirection;

void main()
{
	// We are rendering a full-screen quad, so we calculate a view ray
	// for each corner that will be interpolated for each fragment.
	vec4 ray_clip = vec4(aPos.xy, -1.0, 1.0);
	vec4 ray_eye = u_invProjection * ray_clip;
	ray_eye = vec4(ray_eye.xy, -1.0, 0.0);

	v_rayDirection = (u_invView * ray_eye).xyz;

	// Output a full-screen quad that is at the very back of the clipping volume.
	gl_Position = vec4(aPos.xy, 1.0, 1.0);
}
