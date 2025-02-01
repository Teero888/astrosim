#version 330 core
layout(location = 0) in vec2 aPos;

uniform float Scale;
uniform vec2 Offset;
uniform float ScreenRatio;

out vec2 uv;

void main() {
	uv = aPos;
    gl_Position = vec4((vec2(aPos.x / ScreenRatio, aPos.y) * Scale + Offset), 1.0, 1.0);
}
