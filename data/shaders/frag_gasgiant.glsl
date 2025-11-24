#version 330 core

in vec3 vLocalPos;
in vec3 vRayOrigin;

out vec4 FragColor;

uniform vec3 uLightDir;
uniform vec3 uBaseColor;
uniform vec3 uBandColor;
uniform float uTime;
uniform float uWindSpeed;
uniform float uTurbulence;
uniform float uSeed;
uniform float u_logDepthF;

float hash(vec3 p)
{
	p = fract(p * 0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3D Value Noise with quintic interpolation (Smooth)
float noise(vec3 x)
{
	vec3 i = floor(x);
	vec3 f = fract(x);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

	float a = hash(i + vec3(0, 0, 0));
	float b = hash(i + vec3(1, 0, 0));
	float c = hash(i + vec3(0, 1, 0));
	float d = hash(i + vec3(1, 1, 0));
	float e = hash(i + vec3(0, 0, 1));
	float f_ = hash(i + vec3(1, 0, 1));
	float g = hash(i + vec3(0, 1, 1));
	float h = hash(i + vec3(1, 1, 1));

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = e - a;
	float k4 = a - b - c + d;
	float k5 = a - c - e + g;
	float k6 = a - b - e + f_;
	float k7 = -a + b + c - d + e - f_ - g + h;

	return k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z;
}

float fbm(vec3 p)
{
	float f = 0.0;
	float a = 0.5;
	mat3 m = mat3(
		0.00, 0.80, 0.60,
		-0.80, 0.36, -0.48,
		-0.60, -0.48, 0.64);
	for(int i = 0; i < 2; i++)
	{
		f += a * noise(p);
		p = m * p * 2.02;
		a *= 0.5;
	}
	return f;
}

// Curl of noise field -> Divergence free turbulence
vec3 curl(vec3 p)
{
	float e = 0.1;
	vec3 dx = vec3(e, 0.0, 0.0);
	vec3 dy = vec3(0.0, e, 0.0);
	vec3 dz = vec3(0.0, 0.0, e);

	float x0 = noise(p - dx);
	float x1 = noise(p + dx);
	float y0 = noise(p - dy);
	float y1 = noise(p + dy);
	float z0 = noise(p - dz);
	float z1 = noise(p + dz);

	float x = y1 - y0 - (z1 - z0);
	float y = z1 - z0 - (x1 - x0);
	float z = x1 - x0 - (y1 - y0);

	return normalize(vec3(x, y, z));
}

vec3 GetVelocity(vec3 p)
{
	float lat = p.y;

	// Sharp Zonal Jets (Jupiter style)
	// Alternating direction + varying speed
	// High frequency bands
	float bands = sin(lat * 12.0 + uSeed);

	// Add some noise to bands so they aren't perfect rings
	float warp = noise(p * 1.5);
	bands = sin((lat + warp * 0.1) * 12.0 + uSeed);

	vec3 ZoneDir = vec3(-p.z, 0.0, p.x);

	// Turbulence
	vec3 turb = curl(p * 2.0 + uSeed);

	return normalize(ZoneDir) * bands * uWindSpeed * 2.0 + turb * uTurbulence;
}

// Advection with reset to prevent stretching
float GetAdvectedPattern(vec3 p)
{
	float cycle = 30.0;
	float t = uTime * 0.0005;

	float phase1 = fract(t / cycle);
	float phase2 = fract(t / cycle + 0.5);

	float blend = abs(2.0 * phase1 - 1.0);

	vec3 v = GetVelocity(p);
	float d = 2.0; // Distortion scale

	vec3 p1 = p - v * (phase1 - 0.5) * d;
	vec3 p2 = p - v * (phase2 - 0.5) * d;

	float n1 = fbm(p1 * 2.0 + fbm(p1 * 4.0));
	float n2 = fbm(p2 * 2.0 + fbm(p2 * 4.0));

	return mix(n1, n2, blend);
}

vec4 GetSample(vec3 p)
{
	float r = length(p);
	if(r > 1.0)
		return vec4(0.0);
	float baseDensity = smoothstep(1.0, 0.97, r);

	float n = GetAdvectedPattern(p);

	// Color Banding
	// Use the advected noise to distort the latitude lookup
	float lat = p.y + (n - 0.5) * 0.5 * uTurbulence;
	float band = sin(lat * 15.0 + uSeed);

	// 3-Tone Coloring
	// Base -> Band -> Deep
	vec3 deepColor = uBandColor * 0.5 + vec3(0.05, 0.0, 0.0); // Darker, slightly redder

	vec3 col = mix(uBaseColor, uBandColor, smoothstep(-0.2, 0.2, band));

	// Add "Deep" turbulence contrast (Storms)
	// Map high density noise to deep color
	float storm = smoothstep(0.4, 0.9, n);
	col = mix(col, deepColor, storm);

	// Highlight ridges (lighter tops of clouds)
	float ridge = clamp(1.0 - abs(n * 2.0 - 1.0), 0.0, 1.0);
	col += uBaseColor * 0.15 * ridge * smoothstep(0.5, 1.0, band);

	float den = baseDensity * (0.8 + 1.2 * n);
	return vec4(col, den);
}

vec2 RaySphereIntersect(vec3 ro, vec3 rd, float radius)
{
	float b = dot(ro, rd);
	float c = dot(ro, ro) - radius * radius;
	float d = b * b - c;
	if(d < 0.0)
		return vec2(-1.0);
	float sqrtD = sqrt(d);
	return vec2(-b - sqrtD, -b + sqrtD);
}

void main()
{
	vec3 ro = vRayOrigin;
	vec3 rd = normalize(vLocalPos - vRayOrigin);

	vec2 t = RaySphereIntersect(ro, rd, 1.0);
	if(t.y < 0.0)
		discard;

	float tNear = max(0.0, t.x);

	bool inside = (t.x < 0.0);
	float tFar = min(t.y, tNear + (inside ? 1.0 : 0.2));

	int steps = 24;
	float stepSize = (tFar - tNear) / float(steps);

	float dither = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
	float currentT = tNear + stepSize * dither;

	vec4 accum = vec4(0.0);
	vec3 lightDir = normalize(uLightDir);

	for(int i = 0; i < steps; i++)
	{
		if(accum.a >= 0.95)
			break; // Early exit

		vec3 p = ro + rd * currentT;
		if(length(p) <= 1.0)
		{
			vec4 data = GetSample(p);
			float d = data.a;

			if(d > 0.001)
			{
				vec3 normal = normalize(p);
				float diff = max(dot(normal, lightDir), 0.0);
				float shadow = exp(-d * 3.0); // Beer's law

				vec3 light = vec3(diff * 0.7 + 0.3);
				vec3 rgb = data.rgb * light * shadow;

				float alpha = 1.0 - exp(-d * stepSize * 30.0);

				accum.rgb += rgb * alpha * (1.0 - accum.a);
				accum.a += alpha * (1.0 - accum.a);
			}
		}
		currentT += stepSize;
	}

	if(accum.a < 0.01)
		discard;

	vec3 finalColor = accum.rgb / (accum.rgb + vec3(1.0));
	finalColor = pow(finalColor, vec3(1.0 / 2.2));
	FragColor = vec4(finalColor, accum.a);

	float w = 1.0 / gl_FragCoord.w;
	gl_FragDepth = log2(w + 1.0) * u_logDepthF * 0.5;
}
