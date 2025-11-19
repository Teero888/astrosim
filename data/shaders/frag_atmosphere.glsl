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
// INCREASED SAMPLES FOR SMOOTHER HORIZONS
const int NUM_SAMPLES = 64;
const int NUM_LIGHT_SAMPLES = 8; 
const float PI = 3.141592653589793;

// Optimized: Uses standard quadratic formula (b^2 - c) to avoid cross products
vec2 raySphereIntersect(vec3 ro, vec3 rd, float rad)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    float d = b * b - c;
    
    if (d < 0.0) return vec2(1e5, -1e5);
    
    float sqrtD = sqrt(d);
    return vec2(-b - sqrtD, -b + sqrtD);
}

// Optimized: Boolean check only, avoids sqrt entirely
bool testShadow(vec3 ro, vec3 rd, float rad)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    float d = b * b - c;
    // If d < 0, we miss. 
    // If b > 0, the sphere is behind the ray origin (since ro is outside/above sphere).
    return d >= 0.0 && b <= 0.0;
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
    float depthCorrection = length(v_viewRay);
    float linearDist = planarDepth * depthCorrection;
    
    // If depth is effectively infinite (Skybox/Stars), push it out
    if (planarDepth > 0.99 * 1e30) { 
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
    
    // If we miss the atmosphere, early out
    if(atmosphereIntersect.y < 0.0) discard;

    float tStart = max(0.0, atmosphereIntersect.x);
    float tEnd = atmosphereIntersect.y;

    // Calculate the Analytical End Point (The "Perfect" Geometry)
    // Optimized: Inline the check to avoid function call overhead if possible, 
    // but the cached result is usually fine.
    vec2 planetIntersect = raySphereIntersect(u_cameraPos, rayDir, earthR);
    if (planetIntersect.y > 0.0 && planetIntersect.x > 0.0) {
        tEnd = min(tEnd, planetIntersect.x);
    }

    // 4. Occlusion Logic
    tEnd = min(tEnd, sceneDist);

    // Safety: If we are fully occluded
    if(tStart >= tEnd) {
        FragColor = vec4(0.0);
        return;
    }

    // 5. Pre-calculate Phase and Coefficients (Invariant Hoisting)
    // Moving these out of the loop saves calculation per sample
    float mu = dot(rayDir, sunDir);
    float mu2 = mu * mu;
    float phaseR = 3.0 / (16.0 * PI) * (1.0 + mu2);
    float g = u_miePreferredScatteringDir;
    float g2 = g * g;
    float phaseM = 3.0 / (8.0 * PI) * ((1.0 - g2) * (1.0 + mu2)) / ((2.0 + g2) * pow(1.0 + g2 - 2.0 * g * mu, 1.5));
    
    vec3 betaR = u_rayleighScatteringCoeff * u_realPlanetRadius;
    vec3 betaM = u_mieScatteringCoeff * 1.1 * u_realPlanetRadius;

    // 6. Raymarch Loop
    // Optimized: Simple linear addition rather than re-normalizing
    vec3 startPos = u_cameraPos + rayDir * tStart;
    
    float rayLen = tEnd - tStart;
    float stepSize = rayLen / float(NUM_SAMPLES);
    float halfStep = stepSize * 0.5;
    
    vec3 totalOpticalDepthR = vec3(0.0);
    vec3 totalOpticalDepthM = vec3(0.0);
    vec3 accumR = vec3(0.0);
    vec3 accumM = vec3(0.0);
    float currentDist = 0.0;

    // Pre-calc light step size ratio to avoid division in inner loop
    float invNumLightSamples = 1.0 / float(NUM_LIGHT_SAMPLES);

    for(int i = 0; i < NUM_SAMPLES; i++)
    {
        vec3 samplePos = startPos + rayDir * (currentDist + halfStep);
        float height = max(0.0, length(samplePos) - earthR);

        float densityR = GetDensity(height, u_rayleighScaleHeight);
        float densityM = GetDensity(height, u_mieScaleHeight);
        
        totalOpticalDepthR += densityR * stepSize;
        totalOpticalDepthM += densityM * stepSize;

        // Optimized: Shadow check without sqrt
        if (!testShadow(samplePos, sunDir, earthR))
        {
            vec2 lightIntersect = raySphereIntersect(samplePos, sunDir, atmR);
            float lightRayLen = lightIntersect.y; // Distance to atmosphere edge
            float lightStep = lightRayLen * invNumLightSamples;
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

            // Combine optical depths
            // Note: totalOpticalDepth is already multiplied by stepSize in accumulation, 
            // but for attenuation we need the sum along the path so far.
            // Wait: In original code, totalOpticalDepth was accumulated. 
            // Correct logic: Optical Depth = Density * Length. 
            
            vec3 tau = betaR * (totalOpticalDepthR + lightOpticalDepthR * lightStep) +
                       betaM * (totalOpticalDepthM + lightOpticalDepthM * lightStep);

            vec3 attenuation = exp(-tau);
            
            accumR += densityR * attenuation; // stepSize applied at end
            accumM += densityM * attenuation;
        }
        currentDist += stepSize;
    }

    // Apply stepSize once at the end to save multiplications
    accumR *= stepSize;
    accumM *= stepSize;

    vec3 inscatter = (accumR * betaR * phaseR + accumM * betaM * phaseM) * u_sunIntensity;

    // 1. Remove arbitrary dimmer (* 0.4) and use raw inscatter
    vec3 color = inscatter;
    
    // 2. Switch to Simple Reinhard Tone Mapping (Matches frag_body.glsl)
    color = color / (color + vec3(1.0));
    
    // 3. Gamma Correct
    color = pow(color, vec3(1.0/2.2));

    // Calculate alpha/transmittance based on full path extinction
    vec3 totalExtinction = exp(-(betaR * totalOpticalDepthR + betaM * totalOpticalDepthM));
    float transmittance = (totalExtinction.r + totalExtinction.g + totalExtinction.b) / 3.0;
    
    float alpha = 1.0 - transmittance;

    FragColor = vec4(color, alpha);
}
