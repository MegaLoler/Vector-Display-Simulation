#version 330 core

out vec2 uv;

layout (location = 0) in vec2 position;

void main () {
    uv = position.xy / 2. + 0.5;
    gl_Position = vec4 (position, 0., 1.);
}
