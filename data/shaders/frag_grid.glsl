#version 460 core
#extension GL_ARB_gpu_shader_fp64 : require

in vec3 v_rayDirection;

out vec4 FragColor;

// For ray-casting
uniform dvec3 u_cameraPos;
uniform float u_gridPlaneY;

// For depth calculation
uniform mat4 u_renderView;
uniform mat4 u_renderProjection;
uniform float u_logDepthF;

uniform vec3 u_gridColor;
uniform float u_viewDistance;

// For planet cutout
uniform dvec3 u_focusedBodyPos;
uniform float u_focusedBodyRadius;

void main()
{
	dvec3 rayDir = normalize(dvec3(v_rayDirection));

	// Discard if ray is parallel to the plane or pointing away from it.
	if(sign(u_cameraPos.y - u_gridPlaneY) == sign(rayDir.y))
		discard;
	if(abs(rayDir.y) < 1e-6)
		discard;

	// t = (plane_y - camera_y) / ray_dir_y
	double t = (double(u_gridPlaneY) - u_cameraPos.y) / rayDir.y;

	if(t < 0.0)
		discard; // Intersection is behind the camera

	// Calculate the world-space intersection point
	dvec3 worldPos = u_cameraPos + t * rayDir;

	// Planet Cutout {{{
	// Create a smooth fade around the planet instead of a sharp discard.
	double distToBodyCenter = distance(worldPos, u_focusedBodyPos);
	double radius = double(u_focusedBodyRadius);
	// The hole is fully transparent inside 1.1x radius.
	// The fade occurs between 1.4x and 1.1x the planet's radius.
	double holeEdge = radius * 1.4;
	double fadeEdge = radius * 2.0;
	// Calculate a linear fade factor.
	double planetFade = clamp((distToBodyCenter - holeEdge) / (fadeEdge - holeEdge), 0.0, 1.0);
	// }}}

	// Depth Calculation {{{
	// Calculate the correct depth for the fragment so planets can occlude it.
	dvec3 relativePos = worldPos - u_cameraPos;
	vec4 clipPos = u_renderProjection * u_renderView * vec4(vec3(relativePos), 1.0);

	// Convert linear depth to logarithmic depth and write to gl_FragDepth
	float log_ndc_z = (log2(max(1e-6, clipPos.w) + 1.0)) * u_logDepthF - 1.0;
	gl_FragDepth = (log_ndc_z + 1.0) / 2.0;
	// }}}

	// Grid line drawing logic {{{
	double camDistToPlane = abs(u_viewDistance);
	double grid_scale = double(pow(10.0, floor(log(float(camDistToPlane)) / log(10.0)) - 1.0));

	// Minor grid lines
	dvec2 coord = worldPos.xz / grid_scale;
	dvec2 grid = abs(fract(coord - 0.5) - 0.5);
	vec2 f = fwidth(vec2(coord));
	vec2 grid_line = smoothstep(f * 0.75, f * 1.25, vec2(grid));
	float min_grid = min(grid_line.x, grid_line.y);

	// Major grid lines (10x larger)
	dvec2 coord_major = worldPos.xz / (grid_scale * 10.0);
	dvec2 grid_major = abs(fract(coord_major - 0.5) - 0.5);
	vec2 f_major = fwidth(vec2(coord_major));
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

	FragColor = vec4(u_gridColor, final_alpha);
}
