#version 330 core
precision highp float;

in vec3 FragWorldPos;
in vec3 FragPos;

out vec4 FragColor;

uniform float GridScale;
uniform vec3 GridColor;

void main()
{
    vec2 Coord = FragWorldPos.xz / GridScale;
    vec2 Grid = abs(fract(Coord - 0.5) - 0.5);

    float MinGrid = min(Grid.x, Grid.y);
    float GridLine = smoothstep(0.0, 0.1, MinGrid);

    if (GridLine > 0.05)
        discard;

    FragColor = vec4(GridColor, 1.0 - (length(FragPos) / 10.0));
}
