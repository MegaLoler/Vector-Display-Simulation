#include <iostream>
#include <fstream>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

double noise () {
    return (double) rand () / RAND_MAX;
}

// 2d vector representation
struct vec2 {
    double x;
    double y;
    vec2 (double x, double y) : x (x), y (y) {}
};

// power supply parameters
const double power_supply_smoothing = 10;   // per frame

// electron beam parameters
const int electron_count = 10000;           // per frame
const double electron_intensity = 100;      // total energy emitted per frame
const double electron_scattering = 0.5;     // impurity of the beam

// phosphor parameters
const double phosphor_persistence = 10;     // divides how much emittance remains after one frame
const double phosphor_reflectance = 0.01;   // color floor
// TODO: define phosphor color here

// bloom parameters
const int bloom_kernel_size = 30;
const double bloom_brightness = 15;
const double bloom_spread = 500;

// screen dimensions
// TODO: allow screen resizing
const int width = 480;
const int height = 360;
const int size = width * height;

// precalculations
const double intensity_per_electron = electron_intensity / electron_count;
const double electron_delta = 1.0 / electron_count;
const double phosphor_decay = 1.0 / (1 + phosphor_persistence);
const double power_supply_decay = 1.0 / (1 + power_supply_smoothing);

// state variables
double power_supply_in = 1;     // power input; 1 = normal, 0 = off
double power_supply_out = 0;    // smoothed output of power supply

// electron buffer
// new electrons hitting the screen
// adjusted for decay after time hit between current and last frame
double electron_buffer[size];

// phosphor buffer
// total emittance of phosphor at each pixel
double phosphor_buffer[size];

// opengl stuff
GLuint vbo, vao, program;

// sample the path for the electron beam to trace per frame
// 0 <= n <= 1
vec2 sample_path (double n) {
    //TMP circle
    const double r = 20;
    const double x = 100;
    const double y = 100;
    double a = n * M_PI * 2;
    return vec2 (cos (a) * r + x, sin (a) * r + y);
}

void render () {

    // TODO: make unit time 1 second and incorporate variable delta time

    // update the power supply
    // TODO: integrate power supply out per electron instead of per frame for more accuracy
    power_supply_out += (power_supply_in - power_supply_out) * power_supply_decay;

    // prepare the electron buffer
    std::fill_n (electron_buffer, size, 0); // clear it first
    for (double n = 0; n < 1; n += electron_delta) {

        // sample the ideal point on the path to be traced
        vec2 point = sample_path (n);

        // calculate random scattering
        double offset_radius = tan (noise () * 2) * electron_scattering;
        double offset_angle = noise() * M_PI * 2;
        double offset_x = cos (offset_angle) * offset_radius;
        double offset_y = sin (offset_angle) * offset_radius;

        // calculate final dot position
        int x = (point.x + offset_x) * power_supply_out;
        int y = (point.y + offset_y) * power_supply_out;

        // clip
        if (x < 0 || y < 0 || x >= width || y >= height)
            continue;

        // calculate intensity and adjust for decay at this time
        // TODO: idk a good curve, find a better one?
        double decay_curve = 1 - n * n;
        double intensity = intensity_per_electron * power_supply_out;
        intensity -= intensity_per_electron * phosphor_decay * decay_curve;

        // plot the result on the electron buffer
        electron_buffer[x + y * width] += intensity;
    }

    // update the phosphor buffer
    // TODO: convert to shader for speed
    for (int i = 0; i < size; i++) {
        phosphor_buffer[i] += (electron_buffer[i] - phosphor_buffer[i]) * phosphor_decay;
    }

    // render the phosphor buffer with bloom filter
    // TODO: write shaders
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
    // create the full screen quad
    float vertices[] = {
        -1, -1,
        1, -1,
        -1, 1,
        1, 1,
    };
    glGenBuffers (1, &vbo);
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof (float), (void *) 0);
    glEnableVertexAttribArray(0);
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
        std::cerr << "Could not compile vertex shader:\n" << log << std::endl;
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
