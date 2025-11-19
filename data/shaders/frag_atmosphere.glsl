#version 460 core

in vec3 v_rayDirection;
in vec3 v_viewRay;
out vec4 FragColor;

// Texture Inputs
uniform sampler2D u_depthTexture;
uniform vec2 u_screenSize;

// Unit Space Inputs
uniform vec3 u_cameraPos; 
uniform vec3 u_sunDirection;
uniform float u_atmosphereRadius; 

// Physics Input
uniform float u_realPlanetRadius; 

// Scattering Params
uniform vec3 u_rayleighScatteringCoeff;
uniform float u_rayleighScaleHeight; 
uniform vec3 u_mieScatteringCoeff;
uniform float u_mieScaleHeight; 
uniform float u_miePreferredScatteringDir;

// Customizable Intensity
uniform float u_sunIntensity; 

// Logarithmic Depth Constant
uniform float u_logDepthF;

// CONFIG
const int NUM_SAMPLES = 32;
const int NUM_LIGHT_SAMPLES = 4; 
const float PI = 3.141592653589793;

vec2 raySphereIntersect(vec3 ro, vec3 rd, float rad)
{
    float b = dot(ro, rd);
    vec3 temp = cross(ro, rd);
    float h_sq = dot(temp, temp); 
    float d = rad * rad - h_sq;
    if (d < 0.0) return vec2(1e5, -1e5);
    float sqrtD = sqrt(d);
    return vec2(-b - sqrtD, -b + sqrtD);
}

float GetDensity(float h, float H)
{
    return exp(-max(0.0, h) / H);
}

void main()
{
    // 1. Read Depth and Convert to Linear PLANAR Distance (View Z)
    vec2 uv = gl_FragCoord.xy / u_screenSize;
    float logZ = texture(u_depthTexture, uv).r;
    float ndcZ = logZ * 2.0 - 1.0;
    // Recover View Space Z
    float planarDepth = exp2((ndcZ + 1.0) / u_logDepthF) - 1.0;

    // 2. Convert Planar Depth to Ray Euclidean Distance
    // The depth buffer stores distance along the camera Z axis.
    // The ray travels along the hypotenuse.
    // Factor = length(viewRay) / abs(viewRay.z). Since viewRay.z is -1.0, factor is length.
    float depthCorrection = length(v_viewRay);
    float linearDist = planarDepth * depthCorrection;
    
    // If depth is effectively infinite (Skybox/Stars), push it out
    if (planarDepth > 0.99 * 1e30) { // 1e10 is FAR_PLANE
        linearDist = 1e38;
    }

    // Normalize to Planet Radius Units (Unit Space)
    float sceneDist = linearDist / u_realPlanetRadius;

    // 3. Raymarch Setup
    vec3 rayDir = normalize(v_rayDirection);
    vec3 sunDir = normalize(u_sunDirection);
    float earthR = 1.0;
    float atmR = u_atmosphereRadius;

    vec2 atmosphereIntersect = raySphereIntersect(u_cameraPos, rayDir, atmR);
    vec2 planetIntersect = raySphereIntersect(u_cameraPos, rayDir, earthR);

    // If we miss the atmosphere, early out
    if(atmosphereIntersect.y < 0.0) discard;

    float tStart = max(0.0, atmosphereIntersect.x);
    float tEnd = atmosphereIntersect.y;

    // Calculate the Analytical End Point (The "Perfect" Geometry)
    if (planetIntersect.y > 0.0 && planetIntersect.x > 0.0) {
        tEnd = min(tEnd, planetIntersect.x);
    }

    // 4. Occlusion Logic
    // If the buffer is closer than the analytical hit, it's a separate object (Ship/Moon/Terrain).
    if (sceneDist < tEnd) {
        tEnd = sceneDist;
    }

    // Safety: If we are fully occluded
    if(tStart >= tEnd) {
        FragColor = vec4(0.0);
        return;
    }

    // 5. Raymarch Loop
    // Precision Snap: Reconstruct start position to minimize jitter
    vec3 startPos = (tStart > 0.0) ? (normalize(u_cameraPos + rayDir * tStart) * atmR) : u_cameraPos;
    
    float rayLen = tEnd - tStart;
    float stepSize = rayLen / float(NUM_SAMPLES);
    
    vec3 totalOpticalDepthR = vec3(0.0);
    vec3 totalOpticalDepthM = vec3(0.0);
    vec3 accumR = vec3(0.0);
    vec3 accumM = vec3(0.0);
    float currentDist = 0.0;

    for(int i = 0; i < NUM_SAMPLES; i++)
    {
        vec3 samplePos = startPos + rayDir * (currentDist + stepSize * 0.5);
        float height = max(0.0, length(samplePos) - earthR);

        float densityR = GetDensity(height, u_rayleighScaleHeight);
        float densityM = GetDensity(height, u_mieScaleHeight);
        totalOpticalDepthR += densityR * stepSize;
        totalOpticalDepthM += densityM * stepSize;

        vec2 earthShadow = raySphereIntersect(samplePos, sunDir, earthR);
        if (!(earthShadow.y > 0.0 && earthShadow.x > 0.0))
        {
            vec2 lightIntersect = raySphereIntersect(samplePos, sunDir, atmR);
            float lightStep = lightIntersect.y / float(NUM_LIGHT_SAMPLES);
            float lightOpticalDepthR = 0.0;
            float lightOpticalDepthM = 0.0;
            float lightT = lightStep * 0.5;

            for(int j = 0; j < NUM_LIGHT_SAMPLES; j++)
            {
                vec3 lightPos = samplePos + sunDir * lightT;
                float lh = max(0.0, length(lightPos) - earthR);
                lightOpticalDepthR += GetDensity(lh, u_rayleighScaleHeight);
                lightOpticalDepthM += GetDensity(lh, u_mieScaleHeight);
                lightT += lightStep;
            }

            vec3 tau = (u_rayleighScatteringCoeff * (totalOpticalDepthR + lightOpticalDepthR * lightStep) +
                       u_mieScatteringCoeff * 1.1 * (totalOpticalDepthM + lightOpticalDepthM * lightStep)) * u_realPlanetRadius;

            vec3 attenuation = exp(-tau);
            accumR += densityR * attenuation * stepSize;
            accumM += densityM * attenuation * stepSize;
        }
        currentDist += stepSize;
    }

    float mu = dot(rayDir, sunDir);
    float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu * mu);
    float g = u_miePreferredScatteringDir;
    float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5));

    vec3 inscatter = (accumR * u_rayleighScatteringCoeff * phaseR + accumM * u_mieScatteringCoeff * phaseM) * u_realPlanetRadius * u_sunIntensity;

    vec3 color = inscatter * 0.4; 
    color = clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);

    vec3 totalExtinction = exp(-(u_rayleighScatteringCoeff * totalOpticalDepthR + u_mieScatteringCoeff * 1.1 * totalOpticalDepthM) * u_realPlanetRadius);
    float transmittance = (totalExtinction.r + totalExtinction.g + totalExtinction.b) / 3.0;
    
    // Strict physics blending: 
    // FinalColor = AtmosphereColor + BackgroundColor * Transmittance
    // OpenGL BlendFunc(ONE, ONE_MINUS_SRC_ALPHA) -> Src + Dst * (1 - SrcAlpha)
    // Therefore: SrcAlpha = 1.0 - Transmittance
    float alpha = 1.0 - transmittance;

    FragColor = vec4(color, alpha);
}
