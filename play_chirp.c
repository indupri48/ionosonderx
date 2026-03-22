#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "portaudio.h"
#include <string.h>

#define DURATION_S 30
#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 1024

#ifndef M_PI
#define M_PI 3.14159265
#endif

#define TABLE_SIZE 44100
typedef struct {
    float samples[TABLE_SIZE];
    int index;
}
context_t;

static int patestCallback(
    const void *in_buffer,
    void *out_buffer,
    unsigned long frames_per_buffer,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags flags,
    void *context
) {

    context_t *data = (context_t *)context;
    float *output_buffer = (float *)out_buffer;
    // printf("%f\n", data->samples[1000]);

    float chirp_amplitude = 0.5;
    float noise_amplitude = 0.5;
    for(int i = 0; i < frames_per_buffer; i++) {
        
        float sample = chirp_amplitude * data->samples[data->index];
        sample += noise_amplitude * (2.0 * (float)rand() / RAND_MAX - 1.0);

        *output_buffer++ = sample; // left 
        *output_buffer++ = sample; // right
        
        data->index = (data->index + 1) % TABLE_SIZE;
    }
    // float a, b;
    // memcpy(&a , (float *)out_buffer + 2000, sizeof(float));
    // memcpy(&b , (float *)out_buffer + 2001, sizeof(float));
    // printf("%f %f\n", a, b);
    
    return paContinue;

}

int main() {
    
    // Initialise the sample LUT which will be looped over for the duration of the stream
    float f0 = 0.0;
    float f1 = 3000.0;
    float chirp_rate = 10000.0;
    float chirp_time = (f1 - f0) / chirp_rate;
    int n_chirp_samples = (int)(chirp_time * SAMPLE_RATE);

    // Check whether the table can contain the desired chirp in its first half
    if(n_chirp_samples > TABLE_SIZE / 2) {
        printf("Table size too small for desired chirp to fit in the first half. Exiting...\n");
        return 1;
    }

    // Generate the chirp samples in the audio stream's context structure
    context_t data;
    for(int i = 0; i < n_chirp_samples; i++) {
        float time = (float)i / SAMPLE_RATE;
        float phase = 0.5 * chirp_rate * time * time;
        data.samples[i] = ((float)i + 1.0) / (n_chirp_samples) *sinf(2.0 * M_PI * phase);
    }

    // Set the second half of the LUT to all zeros
    for(int i = n_chirp_samples; i < TABLE_SIZE; i++) data.samples[i] = 0.0;
    
    // Save the samples to a file for analysis elsewhere
    printf("Saving chirp LUT to file...\n");
    FILE *samples_file = fopen("data/chirp_samples.dat", "wb");
    fwrite(data.samples, sizeof(float), TABLE_SIZE, samples_file);
    fclose(samples_file);

    // Start the looping at index 0
    data.index = 0;

    // Initialise PortAudio
    PaError err;
    err = Pa_Initialize();
    if(err != paNoError) {
        printf("Failed to start PortAudio. Exiting...\n");
        return 1;
    }

    // Search for the input audio device with the target_device as a name
    PaDeviceIndex device_index = -1;
    char *target_device = "Loopback: PCM (hw:2,0)";
    printf("Searching for device \"%s\"...\n", target_device);
    PaDeviceIndex n_devices = Pa_GetDeviceCount();
	for(int i = 0; i < n_devices; i++) {
        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);
        if(!strcmp(device_info->name, target_device)) {
            device_index = (PaDeviceIndex)i;
            printf("Found device \"%s\" (channels: %i, fs: %lfHz)\n", target_device, device_info->maxOutputChannels, device_info->defaultSampleRate);
        }
    }

    // Check whether the device has been found or not. If not, exit the program here.
    // if(device_index < 0) {
    //     printf("Failed to find audio device %s. Exiting...\n");
    //     Pa_Terminate();
    //     return 1;
    // }

    // device_index = Pa_GetDefaultOutputDevice();
    // printf("%s\n", Pa_GetDeviceInfo(device_index)->name);
    
    // The device has been found. Configure the parameters of the stream
    PaStreamParameters outputParameters;
    outputParameters.device = device_index;
    outputParameters.channelCount = 2;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = 1.0; // Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    // Open the audio stream by starting the PortAudio engine
    PaStream *stream;
    err = Pa_OpenStream(
        &stream,
        NULL, // no need for input parameters
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,
        patestCallback,
        &data
    );
    if(err != paNoError) {
        Pa_Terminate();
        printf("The audio stream wasn't opened successfully. Exiting...\n");
        return 1;
    }

    // Finally start the audio stream
    err = Pa_StartStream(stream);
    if(err != paNoError) {
        Pa_Terminate();
        printf("The audio stream was not started successfully. Exiting....\n");
        return 1;
    }

    // Report the starting of the stream and put this thread to sleep for the stream duration
    printf("Playing for %d seconds...\n", DURATION_S);
    for(int i = 0; i < DURATION_S; i++) Pa_Sleep(1000);

    // Stop the stream now
    err = Pa_StopStream( stream );
    if(err != paNoError ) {
        Pa_Terminate();
        printf("Failed to stop the audio stream. Exiting...\n");
        return 1;
    }

    err = Pa_CloseStream(stream);
    if(err != paNoError) {
        Pa_Terminate();
        printf("Failed to close the audio stream. Exiting...\n");
        return 1;
    }

    // Close the PortAudio library
    Pa_Terminate();
    printf("Program complete!\n");
    
    return 0;

}