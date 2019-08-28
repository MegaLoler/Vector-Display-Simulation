#include <iostream>

// 2d vector representation
struct vec2 {
    double x;
    double y;
    vec2 (double x, double y) : x (x), y (y) {}
};

// power supply parameters
const power_supply_smoothing = 10;          // per frame

// electron beam parameters
const int electron_count = 10000;           // per frame
const double electron_intensity = 100;      // total energy emitted per frame
const double electron_scattering = 0.5;     // impurity of the beam

// phosphor parameters
const double phosphor_persistence = 10;     // divides how much emittance remains after one frame
const double phosphor_reflectance = 0.01;   // color floor

// bloom parameters
const int bloom_kernel_size = 30;
const double bloom_brightness = 15;
const double bloom_spread = 500;

// screen dimensions
int width = 480;
int height = 360;
int size = width * height;

// precalculations
const double intensity_per_electron = electron_intensity / electron_count;
const double electron_delta = 1.0 / electron_count;
const double phosphor_decay = 1.0 / (1 + phosphor_peristence);
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

// give new screen size
void update_screen_size (int width, int height) {
    ::width = width;
    ::height = height;
    size = width * height;
}

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
    fill_n (electron_buffer, size, 0); // clear it first
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
        phosphor[i] += (electron_buffer[i] - phosphor[i]) * phosphor_decay;
    }

    // render the phosphor buffer with bloom filter
    // TODO
}

int main (int argc, const char **argv) {

    return 0;
}
