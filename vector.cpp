#include <iostream>
#include <fstream>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

float noise () {
    return (float) rand () / RAND_MAX;
}

// 2d vector representation
struct vec2 {
    float x;
    float y;
    vec2 (float x, float y) : x (x), y (y) {}
};

// power supply parameters
const float power_supply_smoothing = 10;    // per frame

// electron beam parameters
const int electron_count = 10000;           // per frame
const float electron_intensity = 100;       // total energy emitted per frame
const float electron_scattering = 0.5;      // impurity of the beam

// phosphor parameters
const float phosphor_persistence = 10;      // divides how much emittance remains after one frame
const float phosphor_reflectance_red = 0.003;
const float phosphor_reflectance_green = 0.003;
const float phosphor_reflectance_blue = 0.003;
// amber
//const float phosphor_emittance_red = 1;
//const float phosphor_emittance_green = 0.749;
//const float phosphor_emittance_blue = 0;
// green
const float phosphor_emittance_red = 0.2;
const float phosphor_emittance_green = 1;
const float phosphor_emittance_blue = 0;

// bloom parameters
const int bloom_kernel_diameter = 10;
const float bloom_brightness = 10;
const float bloom_spread = 100;

// screen dimensions
// TODO: allow screen resizing
const int width = 480;
const int height = 360;
const int size = width * height;

// precalculations
const float intensity_per_electron = electron_intensity / electron_count;
const float electron_delta = 1.0 / electron_count;
const float phosphor_decay = 1.0 / (1 + phosphor_persistence);
const float power_supply_decay = 1.0 / (1 + power_supply_smoothing);
const int bloom_kernel_radius = bloom_kernel_diameter / 2;
const int bloom_kernel_size = bloom_kernel_diameter * bloom_kernel_diameter;
const float center_x = width / 2.0;
const float center_y = height / 2.0;

// state variables
float power_supply_in = 1;     // power input; 1 = normal, 0 = off
float power_supply_out = 0;    // smoothed output of power supply

// electron buffer
// new electrons hitting the screen
// adjusted for decay after time hit between current and last frame
float electron_buffer[size];

// phosphor buffer
// total emittance of phosphor at each pixel
float phosphor_buffer[size];

// convolution kernel for bloom shader
float kernel[bloom_kernel_size];

// opengl stuff
GLuint vbo, vao, program;
GLuint phosphor_texture, kernel_texture;

// sample the path for the electron beam to trace per frame
// 0 <= n <= 1
vec2 sample_path (float n) {
    //TMP circle
    const float r = 100;
    const float x = 200;
    const float y = 200;
    float a = n * M_PI * 2;
    return vec2 (cos (a) * r + x, sin (a) * r + y);
}

// generate the convolution kernel to pass to the bloom shader
void generate_kernel () {
    for (int i = 0; i < bloom_kernel_size; i++) {
        float x = i % bloom_kernel_diameter;
        float y = i / bloom_kernel_diameter;
        float offset_x = x - bloom_kernel_radius;
        float offset_y = y - bloom_kernel_radius;
        float radius = sqrt (offset_x * offset_x + offset_y * offset_y) / bloom_kernel_diameter;
        float value = pow (radius, 1.0 / bloom_spread);
        kernel[i] = fmax (0, 1 - value);
    }
}

void render () {

    // TODO: make unit time 1 second and incorporate variable delta time

    // update the power supply
    // TODO: integrate power supply out per electron instead of per frame for more accuracy
    power_supply_out += (power_supply_in - power_supply_out) * power_supply_decay;
    float power_supply_out_compliment = 1 - power_supply_out;

    // prepare the electron buffer
    std::fill_n (electron_buffer, size, 0); // clear it first
    for (float n = 0; n < 1; n += electron_delta) {

        // sample the ideal point on the path to be traced
        vec2 point = sample_path (n);

        // calculate random scattering
        float offset_radius = tan (noise () * 2) * electron_scattering;
        float offset_angle = noise() * M_PI * 2;
        float offset_x = cos (offset_angle) * offset_radius;
        float offset_y = sin (offset_angle) * offset_radius;

        // calculate final dot position
        int x = point.x + offset_x;
        int y = point.y + offset_y;
        x += (center_x - x) * power_supply_out_compliment;
        y += (center_y - y) * power_supply_out_compliment;

        // clip
        if (x < 0 || y < 0 || x >= width || y >= height)
            continue;

        // calculate intensity and adjust for decay at this time
        // TODO: idk a good curve, find a better one?
        float decay_curve = 1 - n * n;
        float intensity = intensity_per_electron * power_supply_out;
        intensity -= intensity_per_electron * phosphor_decay * decay_curve;

        // plot the result on the electron buffer
        electron_buffer[x + y * width] += intensity;
    }

    // update the phosphor buffer
    for (int i = 0; i < size; i++) {
        phosphor_buffer[i] += (electron_buffer[i] - phosphor_buffer[i]) * phosphor_decay;
    }

    // render the phosphor buffer with bloom filter
    // TODO: write shaders
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture (GL_TEXTURE_2D, phosphor_texture);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, phosphor_buffer);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture (GL_TEXTURE_2D, kernel_texture);
    glUseProgram (program);
    glBindVertexArray (vao);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
}

std::string read_file (const char *filename) {
    // https://stackoverflow.com/questions/18398167/how-to-copy-a-txt-file-to-a-char-array-in-c
    std::ifstream in (filename);
    std::string contents ((std::istreambuf_iterator <char> (in)),
            std::istreambuf_iterator <char> ());
    return contents;
}

void init_opengl () {
    float vertices[] = {
        -1, -1,
        1, -1,
        -1, 1,
        1, 1,
    };

    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);

    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer (0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof (float), (void *) 0);
    glEnableVertexAttribArray (0);

    glGenTextures (1, &phosphor_texture);
    glBindTexture (GL_TEXTURE_2D, phosphor_texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures (1, &kernel_texture);
    glBindTexture (GL_TEXTURE_2D, kernel_texture);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RED, bloom_kernel_diameter, bloom_kernel_diameter, 0, GL_RED, GL_FLOAT, kernel);

    glBindVertexArray (0);

    // compile shaders
    GLuint vertex_shader, fragment_shader;
    int success;
    const int log_size = 512;
    char log[log_size];
    std::string vertex_shader_source_string = read_file ("shader.vert");
    std::string fragment_shader_source_string = read_file ("shader.frag");
    const char *vertex_shader_source = vertex_shader_source_string.c_str ();
    const char *fragment_shader_source = fragment_shader_source_string.c_str ();
    vertex_shader = glCreateShader (GL_VERTEX_SHADER);
    fragment_shader = glCreateShader (GL_FRAGMENT_SHADER);
    glShaderSource (vertex_shader, 1, &vertex_shader_source, NULL);
    glShaderSource (fragment_shader, 1, &fragment_shader_source, NULL);
    glCompileShader (vertex_shader);
    glCompileShader (fragment_shader);
    glGetShaderiv (vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog (vertex_shader, log_size, NULL, log);
        std::cerr << "Could not compile vertex shader:\n" << log << std::endl;
        exit (EXIT_FAILURE);
    }
    glGetShaderiv (fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog (fragment_shader, log_size, NULL, log);
        std::cerr << "Could not compile fragment shader:\n" << log << std::endl;
        exit (EXIT_FAILURE);
    }

    // create shader program
    program = glCreateProgram ();
    glAttachShader (program, vertex_shader);
    glAttachShader (program, fragment_shader);
    glLinkProgram (program);
    glGetProgramiv (program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog (program, log_size, NULL, log);
        std::cerr << "Could not link shader program:\n" << log << std::endl;
        exit (EXIT_FAILURE);
    }
    glDeleteShader (vertex_shader);
    glDeleteShader (fragment_shader);

    // set parameters
    glUseProgram (program);
    glUniform1i (glGetUniformLocation (program, "kernel_diameter"), bloom_kernel_diameter);
    glUniform1f (glGetUniformLocation (program, "brightness"), bloom_brightness);
    glUniform3f (glGetUniformLocation (program, "reflectance"), phosphor_reflectance_red, phosphor_reflectance_green, phosphor_reflectance_blue);
    glUniform3f (glGetUniformLocation (program, "emittance"), phosphor_emittance_red, phosphor_emittance_green, phosphor_emittance_blue);

    glUniform1i (glGetUniformLocation (program, "source"), 0);
    glUniform1i (glGetUniformLocation (program, "kernel"), 1);

    glUniform2f (glGetUniformLocation (program, "resolution"), width, height);
}

void on_resize (GLFWwindow *window, int width, int height) {
    // TODO: allow for window resizing
    //glViewport (0, 0, width, height);
}

void process_input (GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose (window, true);
}

int main (int argc, const char **argv) {

    generate_kernel ();
    srand (time (0));

    glfwInit();
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow (width, height, "Vector Display Simulator", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Could not create GLFW window" << std::endl;
        glfwTerminate();
        exit (EXIT_FAILURE);
    }
    glfwMakeContextCurrent (window);

    if (!gladLoadGLLoader ((GLADloadproc) glfwGetProcAddress)) {
        std::cerr << "Could not initialize GLAD" << std::endl;
        exit (EXIT_FAILURE);
    }

    glViewport(0, 0, width, height);
    glfwSetFramebufferSizeCallback (window, on_resize);

    init_opengl ();

    while (!glfwWindowShouldClose (window)) {
        process_input (window);
        render ();
        glfwSwapBuffers (window);
        glfwPollEvents ();
    }

    glfwTerminate();
    return 0;
}
