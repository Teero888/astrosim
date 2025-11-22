#version 460 core
#extension GL_ARB_gpu_shader_fp64 : require

in vec3 v_rayDirection;
out vec4 FragColor;

uniform dvec3 u_cameraPos;
uniform dvec3 u_focusedBodyPos;
uniform float u_focusedBodyRadius;

uniform mat4 u_renderView;
uniform mat4 u_renderProjection;
uniform float u_logDepthF;
uniform float u_viewDistance;

void main()
{
	dvec3 rayDir = normalize(dvec3(v_rayDirection));

	double planeY = u_focusedBodyPos.y;
	if(sign(u_cameraPos.y - planeY) == sign(rayDir.y))
		discard;
	if(abs(rayDir.y) < 1e-6)
		discard;

	double t = (planeY - u_cameraPos.y) / rayDir.y;
	if(t < 0.0)
		discard; // Intersection is behind the camera

	dvec3 worldPos = u_cameraPos + t * rayDir;

	// Planet Cutout {{{
	double distToBodyCenter = distance(worldPos, u_focusedBodyPos);
	double radius = double(u_focusedBodyRadius);
	double planetFade = clamp((distToBodyCenter - radius * 1.4) / (radius * 2.0 - radius * 1.4), 0.0, 1.0);
	// }}}

	// Depth Calculation {{{
	dvec3 relativePos = worldPos - u_cameraPos;
	vec4 clipPos = u_renderProjection * u_renderView * vec4(vec3(relativePos), 1.0);
	float log_ndc_z = (log2(max(1e-6, clipPos.w) + 1.0)) * u_logDepthF - 1.0;
	gl_FragDepth = (log_ndc_z + 1.0) / 2.0;
	// }}}

	// Grid line drawing logic {{{
	double camDistToPlane = abs(u_viewDistance);
	double grid_scale = double(pow(10.0, floor(log(float(camDistToPlane)) / log(10.0)) - 1.0));
	dvec3 bodyRelPos = worldPos - u_focusedBodyPos;
	dvec3 camRelPos = worldPos - u_cameraPos;

	// Minor grid lines
	dvec2 coord_pattern = bodyRelPos.xz / grid_scale;
	dvec2 coord_deriv = camRelPos.xz / grid_scale;
	dvec2 grid = abs(fract(coord_pattern - 0.5) - 0.5);

	// Use camera-relative coords for fwidth to avoid float precision jitter
	vec2 f = fwidth(vec2(coord_deriv));
	vec2 grid_line = smoothstep(f * 0.75, f * 1.25, vec2(grid));
	float min_grid = min(grid_line.x, grid_line.y);

	// Major grid lines (10x larger)

	dvec2 coord_major_pattern = bodyRelPos.xz / (grid_scale * 10.0);
	dvec2 coord_major_deriv = camRelPos.xz / (grid_scale * 10.0);
	dvec2 grid_major = abs(fract(coord_major_pattern - 0.5) - 0.5);
	vec2 f_major = fwidth(vec2(coord_major_deriv));
	vec2 grid_line_major = smoothstep(f_major * 0.75, f_major * 1.25, vec2(grid_major));
	float min_grid_major = min(grid_line_major.x, grid_line_major.y);

	// Combine lines
	float line_alpha = (1.0 - min_grid) * 0.25 + (1.0 - min_grid_major) * 0.5;
	// }}}

	// Fading Logic {{{
	// Fade out grid based on the current view distance.
	double fade_start = double(u_viewDistance) * 0.75;
	double fade_end = double(u_viewDistance) * 2.5;
	double distanceFade = 1.0 - smoothstep(fade_start, fade_end, t);
	// }}}

	// Combine all alpha factors
	float final_alpha = clamp(line_alpha, 0.0, 1.0) * float(distanceFade) * float(planetFade);

	if(final_alpha < 0.01)
		discard;

	FragColor = vec4(vec3(0.4), final_alpha);
}
