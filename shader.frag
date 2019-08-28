#version 330 core

out vec4 FragColor;

uniform vec2 resolution;

// bloom parameters
uniform int kernel_diameter;
uniform float brightness;

// phosphor optical properties
uniform vec3 reflectance;
uniform vec3 emittance;

// phosphor texture
uniform sampler2D source;

// convolution kernel texture
uniform sampler2D kernel;

vec3 sample_phosphor (vec2 position) {
    if (position.x < 0 || position.x >= resolution.x || position.y < 0 || position.y >= resolution.y)
        return vec3 (0.0, 0.0, 0.0);
    else
        return texture (source, position / resolution).rrr * emittance + reflectance;
}

float sample_kernel (vec2 position) {
    return texture (kernel, position / kernel_diameter).r;
}

void main () {
    // absolute pixel coordinates
    vec2 position = gl_FragCoord.xy;
    
    // the accumulator
    vec3 color = vec3 (0.0, 0.0, 0.0);

    // if diameter is 0 then disable bloom lol
    if (kernel_diameter > 0) {
        // convolve
        float kernel_radius = kernel_diameter / 2.0;
        for (int i = 0; i < kernel_diameter * kernel_diameter; i++) {
            // get the kernel position
            float x = mod (float (i), kernel_diameter);
            float y = floor (float (i) / kernel_diameter);
            vec2 kernel_position = vec2 (x, y);

            // get the corresponding source position
            vec2 offset = kernel_position - kernel_radius;
            vec2 source_position = position + offset;

            // sample and accumulate
            float kernel_sample = sample_kernel (kernel_position);
            vec3 source_sample = sample_phosphor (source_position);
            color += source_sample * kernel_sample;
        }
    } else {
        color = sample_phosphor (position);
    }

    // adjust brightness
    FragColor = vec4(color * brightness, 1.0f);
} 
