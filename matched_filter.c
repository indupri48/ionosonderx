#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <complex.h>

#include "liquid/liquid.h"

// Custom complex floating-point data type
typedef float __complex__ complex_t;

#define PI 3.1415

// Parameters
int fft_length;
int filter_length;

// For processing

complex_t *fft_in;
complex_t *fft_out;
fftplan fft_plan;
complex_t *ifft_in;
complex_t *ifft_out;
fftplan ifft_plan;

complex_t *filter_taps = NULL;

float *previous_samples = NULL;

void matched_filter_create(float f0, float f1, float rate, float sample_rate) {

    filter_length = (f1 - f0) / rate * sample_rate;
    fft_length = (int)pow(2, ceil(log2(filter_length)) + 2);
    int n_new_samples = fft_length - filter_length + 1;
    fprintf(stderr, "[Matched Filter] FFT Length: %i\n", fft_length);

    // Allocate space for the complex input and output buffers used for correlation
    fft_in = (float complex*) malloc(fft_length * sizeof(float complex));
    fft_out = (float complex*) malloc(fft_length * sizeof(float complex));
    fft_plan = fft_create_plan(fft_length, fft_in, fft_out, LIQUID_FFT_FORWARD, 0);
    ifft_in = (float complex*) malloc(fft_length * sizeof(float complex));
    ifft_out = (float complex*) malloc(fft_length * sizeof(float complex));
    ifft_plan = fft_create_plan(fft_length, ifft_in, ifft_out, LIQUID_FFT_BACKWARD, 0);

    // Generate the complex chirp filter taps
    for(int i = 0; i < filter_length; i++) {
        float t = (float)(filter_length - 1 - i) / sample_rate;
        float p = 2.0 * PI * (f0 * t + 0.5 * rate * t * t);
        fft_in[i] = cexpf(I * p); // a complex chirping sinusoid
    }
    for(int i = filter_length; i < fft_length; i++) fft_in[i] = 0.0; // zero-pad the rest
    FILE *f = fopen("taps.dat", "wb");
    fwrite(fft_in, sizeof(complex_t), fft_length, f);
    fclose(f);

    // Pre-compute the FFT of the taps
    fft_execute(fft_plan);

    // Save the FFT result off into the filter_taps array
    filter_taps = malloc(fft_length * sizeof(float __complex__));
    memcpy(filter_taps, fft_out, fft_length * sizeof(float __complex__));

    // Create space for the tail of each block of samples (and set zeros for the initial values)
    previous_samples = malloc((filter_length - 1) * sizeof(float));
    for(int i = 0; i < filter_length - 1; i++) previous_samples[i] = 0.0;

}

void matched_filter_execute(float *samples_in, float *samples_out) {

    // Do the correlation operation using the overlap-save method

    // Populate the FFT input buffer
    for(int i = 0; i < filter_length - 1; i++) fft_in[i] = previous_samples[i];
    for(int i = filter_length - 1; i < fft_length; i++) fft_in[i] = samples_in[i - (filter_length - 1)]; // audio samples are real only so no "I" component

    fft_execute(fft_plan);
    for(int i = 0; i < fft_length; i++) ifft_in[i]  = fft_out[i] * filter_taps[i];
    fft_execute(ifft_plan);

    for(int i = 0; i < fft_length - (filter_length - 1); i++) samples_out[i] = cabsf(ifft_out[i + filter_length - 1]);

    // Save the newst filter_length-1 samples to the previous_samples buffer
    // printf("%i %i %i\n", fft_length, filter_length, get_n_input_samples());
    memcpy(previous_samples, samples_in + fft_length - 2 * (filter_length - 1), (filter_length - 1) * sizeof(float));

}

void matched_filter_destroy() {

    free(filter_taps);

    // Release the forward FFT resources
    fft_destroy_plan(fft_plan);
    free(fft_in);
    free(fft_out);

    // Release the inverse FFT resources
    fft_destroy_plan(ifft_plan);
    free(ifft_in);
    free(ifft_out);


}

int get_n_input_samples() {

    return fft_length - (filter_length - 1);

}