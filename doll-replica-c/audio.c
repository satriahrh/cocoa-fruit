#include "audio.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static PaStream *audio_stream = NULL;
static int audio_initialized = 0;

// Base64 decoding table (same as in utils.c)
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Initialize audio system
int init_audio(void) {
    if (audio_initialized) {
        return 1; // Already initialized
    }
    
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        printf("❌ Failed to initialize PortAudio: %s\n", Pa_GetErrorText(err));
        return 0;
    }
    
    // Open audio output stream
    err = Pa_OpenDefaultStream(&audio_stream,
                             0,                    // No input channels
                             CHANNELS,             // Output channels
                             AUDIO_FORMAT,         // Sample format
                             SAMPLE_RATE,          // Sample rate
                             FRAMES_PER_BUFFER,    // Frames per buffer
                             NULL,                 // No callback
                             NULL);                // No user data
    
    if (err != paNoError) {
        printf("❌ Failed to open audio stream: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 0;
    }
    
    // Start the stream
    err = Pa_StartStream(audio_stream);
    if (err != paNoError) {
        printf("❌ Failed to start audio stream: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(audio_stream);
        Pa_Terminate();
        return 0;
    }
    
    audio_initialized = 1;
    printf("✅ Audio system initialized (16kHz, Mono, 16-bit)\n");
    return 1;
}

// Cleanup audio system
void cleanup_audio(void) {
    if (audio_stream) {
        Pa_StopStream(audio_stream);
        Pa_CloseStream(audio_stream);
        audio_stream = NULL;
    }
    
    if (audio_initialized) {
        Pa_Terminate();
        audio_initialized = 0;
        printf("🔇 Audio system cleaned up\n");
    }
}

// Play audio data directly
int play_audio_data(const unsigned char *audio_data, size_t data_size) {
    if (!audio_initialized || !audio_stream) {
        printf("❌ Audio system not initialized\n");
        return 0;
    }
    
    printf("🔊 Playing audio (%zu bytes)...\n", data_size);
    
    // Write audio data to stream
    PaError err = Pa_WriteStream(audio_stream, audio_data, data_size / 2); // 2 bytes per sample
    if (err != paNoError) {
        printf("❌ Failed to play audio: %s\n", Pa_GetErrorText(err));
        return 0;
    }
    
    printf("✅ Audio playback completed\n");
    return 1;
}

// Decode base64 audio data
int decode_base64_audio(const char *base64_input, unsigned char **audio_output, size_t *output_size) {
    int input_length = strlen(base64_input);
    int i, j = 0;
    
    // Calculate output size
    *output_size = (input_length * 3) / 4;
    if (base64_input[input_length - 1] == '=') (*output_size)--;
    if (base64_input[input_length - 2] == '=') (*output_size)--;
    
    // Allocate output buffer
    *audio_output = malloc(*output_size);
    if (!*audio_output) {
        printf("❌ Failed to allocate memory for audio data\n");
        return 0;
    }
    
    // Decode base64
    for (i = 0; i < input_length; i += 4) {
        unsigned char byte1 = strchr(base64_chars, base64_input[i]) - base64_chars;
        unsigned char byte2 = (i + 1 < input_length) ? strchr(base64_chars, base64_input[i + 1]) - base64_chars : 0;
        unsigned char byte3 = (i + 2 < input_length) ? strchr(base64_chars, base64_input[i + 2]) - base64_chars : 0;
        unsigned char byte4 = (i + 3 < input_length) ? strchr(base64_chars, base64_input[i + 3]) - base64_chars : 0;
        
        (*audio_output)[j++] = (byte1 << 2) | (byte2 >> 4);
        if (j < *output_size) (*audio_output)[j++] = ((byte2 & 15) << 4) | (byte3 >> 2);
        if (j < *output_size) (*audio_output)[j++] = ((byte3 & 3) << 6) | byte4;
    }
    
    return 1;
}

// Play audio from base64 encoded data
int play_audio_from_base64(const char *base64_audio) {
    unsigned char *audio_data;
    size_t audio_size;
    
    printf("🔍 Decoding base64 audio data...\n");
    
    if (!decode_base64_audio(base64_audio, &audio_data, &audio_size)) {
        printf("❌ Failed to decode base64 audio\n");
        return 0;
    }
    
    int result = play_audio_data(audio_data, audio_size);
    free(audio_data);
    
    return result;
} 