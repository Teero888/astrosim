#version 330 core

in vec3 ScaledFragPos;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform bool Source;
uniform float Radius;
uniform float Time;

vec4 hash(vec4 p) {
    p = vec4( 
        dot(p, vec4(127.1, 311.7, 74.7, 321.3)),
        dot(p, vec4(269.5, 183.3, 246.1, 456.7)),
        dot(p, vec4(113.5, 271.9, 124.6, 789.2)),
        dot(p, vec4(368.4, 198.3, 155.2, 159.7))
    );
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float noise(vec4 p) {
    vec4 i = floor(p);
    vec4 f = fract(p);

    // Calculate interpolation factors
    #if INTERPOLANT==1
    // Quintic interpolant
    vec4 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    #else
    // Cubic interpolant
    vec4 u = f * f * (3.0 - 2.0 * f);
    #endif

    // Calculate contributions from both w layers
    float w0 = mix(
        mix(
            mix(
                mix(dot(hash(i + vec4(0,0,0,0)), f - vec4(0,0,0,0)),
                    dot(hash(i + vec4(1,0,0,0)), f - vec4(1,0,0,0)), u.x),
                mix(dot(hash(i + vec4(0,1,0,0)), f - vec4(0,1,0,0)),
                    dot(hash(i + vec4(1,1,0,0)), f - vec4(1,1,0,0)), u.x), u.y),
            mix(
                mix(dot(hash(i + vec4(0,0,1,0)), f - vec4(0,0,1,0)),
                    dot(hash(i + vec4(1,0,1,0)), f - vec4(1,0,1,0)), u.x),
                mix(dot(hash(i + vec4(0,1,1,0)), f - vec4(0,1,1,0)),
                    dot(hash(i + vec4(1,1,1,0)), f - vec4(1,1,1,0)), u.x), u.y), u.z),
        mix(
            mix(
                mix(dot(hash(i + vec4(0,0,0,1)), f - vec4(0,0,0,1)),
                    dot(hash(i + vec4(1,0,0,1)), f - vec4(1,0,0,1)), u.x),
                mix(dot(hash(i + vec4(0,1,0,1)), f - vec4(0,1,0,1)),
                    dot(hash(i + vec4(1,1,0,1)), f - vec4(1,1,0,1)), u.x), u.y),
            mix(
                mix(dot(hash(i + vec4(0,0,1,1)), f - vec4(0,0,1,1)),
                    dot(hash(i + vec4(1,0,1,1)), f - vec4(1,0,1,1)), u.x),
                mix(dot(hash(i + vec4(0,1,1,1)), f - vec4(0,1,1,1)),
                    dot(hash(i + vec4(1,1,1,1)), f - vec4(1,1,1,1)), u.x), u.y), u.z),
        u.w);

    return w0;
}

void main() {
	if(Source)
	{
    	FragColor = vec4(objectColor * vec3(1.0, (noise(vec4(ScaledFragPos * 2.5, Time)) / 2.0) + 0.5, 1.0), 1.0);
		return;
	}
    // ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // combine lighting
    vec3 result = (ambient + diffuse) * objectColor;
	
    FragColor = vec4(result, 1.0);
}
