#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <portaudio.h>

// Audio functions
int init_audio(void);
void cleanup_audio(void);
int play_audio_data(const unsigned char *audio_data, size_t data_size);
int play_audio_from_base64(const char *base64_audio);

// Audio configuration
#define SAMPLE_RATE 16000
#define CHANNELS 1
#define FRAMES_PER_BUFFER 512
#define AUDIO_FORMAT paInt16

// Base64 decoding for audio
int decode_base64_audio(const char *base64_input, unsigned char **audio_output, size_t *output_size);

#endif // AUDIO_H 