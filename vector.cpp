#include <iostream>
#include <fstream>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// power supply parameters
const float power_supply_smoothing = 10;    // per frame

// electron beam parameters
const int electron_count = 20000;           // per frame
const float electron_intensity = 500;       // total energy emitted per frame
const float electron_scattering = 0.25;     // impurity of the beam

// phosphor parameters
const float phosphor_persistence = 10;      // divides how much emittance remains after one frame
const float phosphor_reflectance_red = 0.003;
const float phosphor_reflectance_green = 0.003;
const float phosphor_reflectance_blue = 0.003;
const bool enable_phosphor_filter = false;
// amber
//const float phosphor_emittance_red = 1;
//const float phosphor_emittance_green = 0.749;
//const float phosphor_emittance_blue = 0;
// green
const float phosphor_emittance_red = 0.2;
const float phosphor_emittance_green = 1;
const float phosphor_emittance_blue = 0.2;

// bloom parameters
const int bloom_kernel_diameter = 0;        // 0 to disable bloom
const float bloom_brightness = 5;
const float bloom_spread = 100;

// screen dimensions
// TODO: allow screen resizing
const int width = 480;
const int height = 360;
const int size = width * height;

// 4x4 matrix representation
struct mat4 {
    float xx;
    float xy;
    float xz;
    float xw;

    float yx;
    float yy;
    float yz;
    float yw;

    float zx;
    float zy;
    float zz;
    float zw;

    float wx;
    float wy;
    float wz;
    float ww;

    mat4 (float xx = 1, float xy = 0, float xz = 0, float xw = 0,
          float yx = 0, float yy = 1, float yz = 0, float yw = 0,
          float zx = 0, float zy = 0, float zz = 1, float zw = 0,
          float wx = 0, float wy = 0, float wz = 0, float ww = 1)
    : xx (xx), xy (xy), xz (xz), xw (xw),
      yx (yx), yy (yy), yz (yz), yw (yw),
      zx (zx), zy (zy), zz (zz), zw (zw),
      wx (wx), wy (wy), wz (wz), ww (ww)
    {}

    mat4 operator * (mat4 m) {
        mat4 result;

        result.xx = xx * m.xx + yx * m.xy + zx * m.xz + wx * m.xw;
        result.xy = xy * m.xx + yy * m.xy + zy * m.xz + wy * m.xw;
        result.xz = xz * m.xx + yz * m.xy + zz * m.xz + wz * m.xw;
        result.xw = xw * m.xx + yw * m.xy + zw * m.xz + ww * m.xw;

        result.yx = xx * m.yx + yx * m.yy + zx * m.yz + wx * m.yw;
        result.yy = xy * m.yx + yy * m.yy + zy * m.yz + wy * m.yw;
        result.yz = xz * m.yx + yz * m.yy + zz * m.yz + wz * m.yw;
        result.yw = xw * m.yx + yw * m.yy + zw * m.yz + ww * m.yw;

        result.zx = xx * m.zx + yx * m.zy + zx * m.zz + wx * m.zw;
        result.zy = xy * m.zx + yy * m.zy + zy * m.zz + wy * m.zw;
        result.zz = xz * m.zx + yz * m.zy + zz * m.zz + wz * m.zw;
        result.zw = xw * m.zx + yw * m.zy + zw * m.zz + ww * m.zw;

        result.wx = xx * m.wx + yx * m.wy + zx * m.wz + wx * m.ww;
        result.wy = xy * m.wx + yy * m.wy + zy * m.wz + wy * m.ww;
        result.wz = xz * m.wx + yz * m.wy + zz * m.wz + wz * m.ww;
        result.ww = xw * m.wx + yw * m.wy + zw * m.wz + ww * m.ww;

        return result;
    }
};

// 2d vector representation
struct vec2 {
    float x;
    float y;
    vec2 (float x = 0, float y = 0) : x (x), y (y) {}
    vec2 map () {
        // map to screen coordinates
        return vec2 ((x + 1) * width / 2, (y + 1) * height / 2);
    }
};

// 3d vector representation
struct vec3 {
    float x;
    float y;
    float z;
    vec3 (float x = 0, float y = 0, float z = 0) : x (x), y (y), z (z) {}
    vec2 project () {
        return vec2 (x / (z + 1), y / (z + 1));
    }
    vec3 operator * (mat4 m) {
        vec3 result;

        result.x = x * m.xx + y * m.xy + z * m.xz;
        result.y = x * m.yx + y * m.yy + z * m.yz;
        result.z = x * m.zx + y * m.zy + z * m.zz;

        return result;
    }
};

mat4 scale (float x, float y, float z) {
    return mat4 (x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1);
}

mat4 translate (float x, float y, float z) {
    return mat4 (1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1);
}

mat4 rotate_z (float angle) {
    return mat4 (cos (angle), -sin (angle), 0, 1, sin (angle), cos (angle), 0, 1, 0, 0, 0, 1, 0, 0, 0, 1);
}

mat4 rotate_y (float angle) {
    return mat4 (cos (angle), 0, sin (angle), 0, 0, 1, 0, 0, -sin (angle), 0, cos (angle), 0, 0, 0, 0, 1);
}

mat4 rotate_x (float angle) {
    return mat4 (1, 0, 0, 0, 0, cos (angle), -sin (angle), 0, 0, sin (angle), cos (angle), 0, 0, 0, 0, 1);
}

// precalculations
const float intensity_per_electron = electron_intensity / electron_count;
const float electron_delta = 1.0 / electron_count;
const float phosphor_decay = 1.0 / (1 + phosphor_persistence);
const float power_supply_decay = 1.0 / (1 + power_supply_smoothing) / electron_count;
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

// the 3d path for the electron beam to trace
vec3 path[1024];
int vertex_count = 0;

// opengl stuff
GLuint vbo, vao, program;
GLuint phosphor_texture, kernel_texture;

float noise () {
    return (float) rand () / RAND_MAX;
}

// sample the path for the electron beam to trace per frame
// project to 2d
// 0 <= n <= 1
vec2 sample_path (float n) {
    if (vertex_count == 0)
        return vec2 ().map ();
    else if (vertex_count == 1)
        return path[0].project ().map ();

    float n2 = n * (vertex_count / 2);
    int i = floor (n2);
    n = n2 - i;
    i *= 2;
    vec3 p1 = path[i];
    vec3 p2 = path[i + 1];
    vec3 delta = vec3 ((p2.x - p1.x) * n, (p2.y - p1.y) * n, (p2.z - p1.z) * n);
    return vec3 (p1.x + delta.x, p1.y + delta.y, p1.z + delta.z).project ().map ();
}

// generate the convolution kernel to pass to the bloom shader
void generate_kernel () {
    if (bloom_kernel_diameter > 0) {
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
}

void prepare_path (float time) {
    vertex_count = 0;

    // rotating cube

    // normal vertices
    vec3 p000 = vec3 (-1, -1, -1);
    vec3 p001 = vec3 (-1, -1, 1);
    vec3 p010 = vec3 (-1, 1, -1);
    vec3 p011 = vec3 (-1, 1, 1);
    vec3 p100 = vec3 (1, -1, -1);
    vec3 p101 = vec3 (1, -1, 1);
    vec3 p110 = vec3 (1, 1, -1);
    vec3 p111 = vec3 (1, 1, 1);

    // transformed vertices
    mat4 transform = mat4 () * scale (0.3, 0.3, 0.3);
    float angle = time * M_PI * 2 / 8;
    transform = transform * rotate_y (angle);
    transform = transform * rotate_x (angle);
    vec3 p000_ = p000 * transform;
    vec3 p001_ = p001 * transform;
    vec3 p010_ = p010 * transform;
    vec3 p011_ = p011 * transform;
    vec3 p100_ = p100 * transform;
    vec3 p101_ = p101 * transform;
    vec3 p110_ = p110 * transform;
    vec3 p111_ = p111 * transform;

    // edges
    path[vertex_count++] = p000_;
    path[vertex_count++] = p001_;
    path[vertex_count++] = p010_;
    path[vertex_count++] = p011_;
    path[vertex_count++] = p100_;
    path[vertex_count++] = p101_;
    path[vertex_count++] = p110_;
    path[vertex_count++] = p111_;

    path[vertex_count++] = p000_;
    path[vertex_count++] = p010_;
    path[vertex_count++] = p001_;
    path[vertex_count++] = p011_;
    path[vertex_count++] = p100_;
    path[vertex_count++] = p110_;
    path[vertex_count++] = p101_;
    path[vertex_count++] = p111_;

    path[vertex_count++] = p000_;
    path[vertex_count++] = p100_;
    path[vertex_count++] = p001_;
    path[vertex_count++] = p101_;
    path[vertex_count++] = p010_;
    path[vertex_count++] = p110_;
    path[vertex_count++] = p011_;
    path[vertex_count++] = p111_;
}

void render (float time) {

    // TODO: make unit time 1 second and incorporate variable delta time

    // create the path to trace
    prepare_path (time);

    // prepare the electron buffer
    std::fill_n (electron_buffer, size, 0); // clear it first
    for (float n = 0; n < 1; n += electron_delta) {

        // update the power supply
        power_supply_out += (power_supply_in - power_supply_out) * power_supply_decay;
        float power_supply_out_compliment = 1 - power_supply_out;

        // sample the ideal point on the path to be traced
        vec2 point = sample_path (n);

        // TODO: add electron gun inertia for curving and overshoots

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
        if (enable_phosphor_filter)
            intensity -= intensity_per_electron * phosphor_decay * decay_curve;

        // plot the result on the electron buffer
        electron_buffer[x + y * width] += intensity;
    }

    // update the phosphor buffer
    for (int i = 0; i < size; i++) {
        if (enable_phosphor_filter)
            phosphor_buffer[i] += (electron_buffer[i] - phosphor_buffer[i]) * phosphor_decay;
        else
            phosphor_buffer[i] = electron_buffer[i];
    }

    // render the phosphor buffer with bloom filter
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

void on_keyboard (GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        power_supply_in = !power_supply_in;
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

    glViewport (0, 0, width, height);
    glfwSetFramebufferSizeCallback (window, on_resize);
    glfwSetKeyCallback (window, on_keyboard);

    init_opengl ();

    while (!glfwWindowShouldClose (window)) {
        process_input (window);
        render (glfwGetTime ());
        glfwSwapBuffers (window);
        glfwPollEvents ();
    }

    glfwTerminate();
    return 0;
}
