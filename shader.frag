#version 330 core

out vec4 FragColor;

in vec2 uv;

// bloom parameters
uniform int kernel_diameter;
uniform float brightness;

// phosphor optical properties
uniform vec3 reflectance;
uniform vec3 emittance;

// phosphor texture
uniform sampler2D source;

void main ()
{
    vec3 color = texture (source, uv).rrr * emittance + reflectance;
    FragColor = vec4(color * brightness, 1.0f);
} 
