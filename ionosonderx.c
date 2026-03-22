#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <complex.h>

#include "portaudio.h"
#include "liquid/liquid.h"

#include "matched_filter.h"

#define PI 3.1415

// Audio sample input buffer
#define INBUF_LENGTH 65536
#define INBUF_BLOCK_LENGTH 4096 
unsigned int inbuf_write_index = 0;
unsigned int inbuf_read_index = 0;
float *in_buffer = NULL;

// CFAR parameters
#define N_GUARD_CELLS 60
#define N_TRAINING_CELLS 100

// Matched filter output buffer
#define MFBUF_LENGTH (2 * (N_GUARD_CELLS + N_TRAINING_CELLS) + 1)
unsigned int mfbuf_write_index = 0;
unsigned int mfbuf_read_index = 0;
float *mf_buffer = NULL;

#define AUDIO_RATE 44100

FILE *sample_file = NULL;

// Live audio stuff
PaStream *stream;
PaStreamParameters input_parameters;
PaStreamParameters output_parameters;
PaStreamFlags stream_flags;

static int audio_callback(const void *input_buffer, void *output_buffer, unsigned long n_samples, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void *context) {

    // if(input_buffer == NULL) return paContinue;
    // if(output_buffer == NULL) return paContinue;
    // int a = flags & paInputUnderflow;
    // int b = flags & paInputOverflow;
    // int c = flags & paOutputUnderflow;
    // int d = flags & paOutputOverflow;

    // printf("%i %i %i %i\n", a, b, c, d);
    // fflush(stderr);

    // Push this block of samples to the circular buffer and also the output stream
    float *in_samples = (float *)input_buffer;
    float *out_samples = (float *)output_buffer;
    int tmp_write_index = inbuf_write_index;
    for(int i = 0; i < n_samples * 2; i++) {
        *out_samples++ = in_samples[i];
        if(i & 1) continue;
        in_buffer[tmp_write_index] = in_samples[i]; // left channel only
        tmp_write_index++;
        if(tmp_write_index == INBUF_LENGTH) tmp_write_index = 0;
    }

    inbuf_write_index = tmp_write_index;

    return paContinue;

}


void main(int argc, char **argv) {

    FILE *out_file = fopen("data/out.dat", "wb");

    // Initialise the program

    // Initialise the matched filter
    matched_filter_create(0, 3000, 10000, (float)AUDIO_RATE);

    // Initialise the matched filter output buffer
    mf_buffer = malloc(MFBUF_LENGTH * sizeof(float));

    if(argc == 2) {

        // The first argument is either "stdin" or a filename
        char *arg1 = argv[1];
        if(!strcmp(arg1, "stdin")) {
            // Use float32 samples input from the standard input
            printf("The program will run in mode: standard input\n");
            sample_file = stdin;
        } else {
            // Open up the given file containing float32 samples
            printf("The program will run in mode: file (%s)\n", arg1);
            sample_file = fopen(arg1, "rb");
        }

    } else {

        // No argument provided. Assume the user wants to run in real tim
        printf("The program will run in mode: real-time\n");

        // Create the ring buffer for samples
        in_buffer = malloc(INBUF_LENGTH * sizeof(float));

        // Open the audio stream
	    PaError pa_err;
        pa_err = Pa_OpenStream(
            &stream,
            &input_parameters,
            &output_parameters,
            (double)AUDIO_RATE,
            INBUF_BLOCK_LENGTH,
            stream_flags,
            audio_callback,
            NULL
        );
        if(pa_err != paNoError) {
            printf("Failed to open the audio stream!\n");
            return;
        }

        // Now start the audio stream
        pa_err = Pa_StartStream(stream);
        if( pa_err != paNoError ) return;

        pa_err = Pa_Initialize();

        if(pa_err != paNoError) return;

        // Configure the audio stream
        PaDeviceIndex input_device_index = Pa_GetDefaultInputDevice();;
        const PaDeviceInfo *input_device_info = Pa_GetDeviceInfo(input_device_index);
        printf("Input device: %s, fs: %lf, n_chans: %i\n", input_device_info->name, input_device_info->defaultSampleRate, input_device_info->maxInputChannels);
        input_parameters.device = input_device_index;
        input_parameters.channelCount = 2;
        input_parameters.sampleFormat = paFloat32;
        input_parameters.suggestedLatency = input_device_info->defaultLowInputLatency;
        input_parameters.hostApiSpecificStreamInfo = NULL;

        // Simply use the default output for the output audio device
        PaDeviceIndex output_device_index = Pa_GetDefaultOutputDevice();
        const PaDeviceInfo *output_device_info = Pa_GetDeviceInfo(output_device_index);
        printf("Output device: %s, fs: %lf, n_chans: %i\n", output_device_info->name, output_device_info->defaultSampleRate, output_device_info->maxOutputChannels);
        PaStreamParameters output_parameters;
        output_parameters.channelCount = 2;
        output_parameters.device = output_device_index;
        output_parameters.sampleFormat = paFloat32;
        output_parameters.hostApiSpecificStreamInfo = NULL;
        output_parameters.suggestedLatency = output_device_info->defaultLowOutputLatency;

    }
    
    // Main processing loop
    int n_samples = get_n_input_samples();

    float false_alarm_rate = 0.01;
    float exponent = -1.0 / N_TRAINING_CELLS;
    float alpha = N_TRAINING_CELLS * (powf(false_alarm_rate, exponent) - 1);

    float last_cut = 0.0;
    int searching = 0;
    int c = 0;
    while(1) {

        float mf_input[n_samples];
        float mf_output[n_samples];

        if(sample_file != NULL) {
            // Simply read the next float. The file is our buffer
            size_t n_bytes_read = fread(mf_input, sizeof(float), n_samples, sample_file);
            if(n_bytes_read != n_samples) break;
        } else {

            // Wait for there to be at least n_samples available to read in the buffer

            int n = (inbuf_write_index + INBUF_LENGTH - inbuf_read_index) % INBUF_LENGTH;
            while(n < n_samples) usleep(25000);
            memcpy(mf_input, in_buffer + inbuf_read_index * sizeof(float), n_samples * sizeof(float));
            inbuf_read_index = (inbuf_read_index + n_samples) % INBUF_LENGTH;

        }

        matched_filter_execute(mf_input, mf_output);
        fwrite(mf_output, sizeof(float), n_samples, out_file);

        // CFAR
        for(int i = 0; i < n_samples; i++) {

            mf_buffer[mfbuf_write_index] = mf_output[i];
            mfbuf_write_index = (mfbuf_write_index + 1) % MFBUF_LENGTH;

            int cut_index = (mfbuf_write_index - N_GUARD_CELLS - N_TRAINING_CELLS + MFBUF_LENGTH) % MFBUF_LENGTH;

            // Work out the mea\n noise from the training cells
            float noise = 0.0;
            for(int j = 0; j < N_TRAINING_CELLS; j++) {
                int k = (cut_index + 1 + N_GUARD_CELLS + j) % MFBUF_LENGTH;
                noise += mf_buffer[k];
                k = (cut_index - 1 - N_GUARD_CELLS - j + MFBUF_LENGTH) % MFBUF_LENGTH;
                noise += mf_buffer[k];
            }
            noise /= N_TRAINING_CELLS * 2;
            
            // Thresholding
            float cell_under_test = mf_buffer[cut_index];
            float a = alpha * noise;

            FILE *t = fopen("data/cfar.dat", "ab");
            fwrite(&a, 4, 1, t);
            fclose(t);
            
            if(cell_under_test >= a) {
                
                // This sample is likely enough to be a hit
                
                // A peak may be several samples wide. Find the maximum point.
                
                if(searching == 0) {
                    // This is the first point of the peak
                    searching = 1;
                    last_cut = 0; // any actual matched filter is non-negative
                }
                
                if(searching != 2 && cell_under_test < last_cut) {
                    // The PREVIOUS cell was the peak. Make a capture now
                    // TODO this algorithm is really susceptable to noise
                    float d = (float)(c - 1);
                    FILE *t = fopen("data/guess.dat", "ab");
                    fwrite(&d, 4, 1, t);
                    fwrite(&last_cut, 4, 1, t);
                    fclose(t);
                    searching = 2;

                    if(sample_file == NULL) {
                        // Report the time the chirp was reported. The DSP means the chirp will have happened slightly sooner but ignore this delay for now
                        // This only really makes sense for the real-time mode of this program

                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        printf("Detected a chirp at %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

                    }

                }

                last_cut = cell_under_test;

            } else {
                searching = 0;
            }

            c++;

        }

    }

    if(sample_file != NULL) {
        if(sample_file != stdin) {
            // Make sure the sample file is closed
            fclose(sample_file);
        }
    } else {

        // Close the audio stream
        PaError pa_err;
        pa_err = Pa_CloseStream(stream);
        if(pa_err != paNoError) return;

        // Free the ring buffer memory
        free(in_buffer);

    }

    matched_filter_destroy();

    fclose(out_file);

    free(mf_buffer);

}