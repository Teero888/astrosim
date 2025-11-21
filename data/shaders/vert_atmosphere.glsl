#version 460 core
layout(location = 0) in vec2 aPos; // Vertices for a -1 to 1 quad

uniform mat4 u_invView;
uniform mat4 u_invProjection;

out vec3 v_rayDirection;
out vec3 v_viewRay;

void main()
{
	// We are rendering a full-screen quad.
	// Unproject the NEAR plane (Z = -1.0) to keep vector magnitude small and manageable.
	vec4 ray_clip = vec4(aPos.xy, -1.0, 1.0);
	vec4 ray_eye = u_invProjection * ray_clip;

	// Perspective divide is crucial here
	ray_eye /= ray_eye.w;

	// Pass the un-rotated view vector (to the near plane) to fragment shader for depth correction
	v_viewRay = ray_eye.xyz;

	// Calculate World Space Ray Direction
	// We use vec4(..., 0.0) to IGNORE the translation part of u_invView.
	// This ensures we get a pure direction vector.
	v_rayDirection = (u_invView * vec4(ray_eye.xyz, 0.0)).xyz;

	// Output a full-screen quad that is at the very back of the clipping volume.
	gl_Position = vec4(aPos.xy, 1.0, 1.0);
}
