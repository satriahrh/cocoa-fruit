#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <portaudio.h>

// Audio functions
int init_audio(void);
void cleanup_audio(void);
int play_audio_data(const unsigned char *audio_data, size_t data_size);
int play_audio_from_base64(const char *base64_audio);

// Audio recording functions
int start_recording(void);
int stop_recording(void);
int get_recorded_audio(unsigned char **audio_data, size_t *data_size);
int encode_audio_to_base64(const unsigned char *audio_data, size_t data_size, char **base64_output);

// Audio configuration
#define SAMPLE_RATE 16000
#define CHANNELS 1
#define FRAMES_PER_BUFFER 512
#define AUDIO_FORMAT paInt16
#define MAX_RECORDING_DURATION 60  // Increased to 60 seconds for longer recordings

// Base64 decoding for audio
int decode_base64_audio(const char *base64_input, unsigned char **audio_output, size_t *output_size);

#endif // AUDIO_H 