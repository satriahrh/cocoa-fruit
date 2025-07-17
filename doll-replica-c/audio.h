#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <portaudio.h>

// Audio functions
 #include <pthread.h>
int init_audio(void);
void cleanup_audio(void);
int play_audio_data(const unsigned char *audio_data, size_t data_size);
int play_audio_from_base64(const char *base64_audio);

// Audio recording functions
int start_recording(void);
int stop_recording(void);
int get_recorded_audio(unsigned char **audio_data, size_t *data_size);
int encode_audio_to_base64(const unsigned char *audio_data, size_t data_size, char **base64_output);

// Real-time streaming functions
typedef void (*audio_chunk_callback)(const unsigned char *chunk, size_t chunk_size);
int start_recording_with_streaming(audio_chunk_callback callback);
int is_recording_active(void);

// Streaming audio playback functions
int start_streaming_audio_playback(void);
int stop_streaming_audio_playback(void);
int is_streaming_audio_active(void);
int play_audio_chunk(const unsigned char *audio_chunk, size_t chunk_size);

// Audio configuration
#define SAMPLE_RATE 8000
#define CHANNELS 1
#define FRAMES_PER_BUFFER 512
#define AUDIO_FORMAT paUInt8  // MULAW is 8-bit unsigned
#define MAX_RECORDING_DURATION 60  // Increased to 60 seconds for longer recordings
#define STREAMING_CHUNK_SIZE (FRAMES_PER_BUFFER * CHANNELS * 1) // 1KB chunks for MULAW (8-bit)

// Streaming audio buffer configuration
#define STREAMING_AUDIO_BUFFER_SIZE (1024 * 1024) // 1MB buffer for streaming audio
#define STREAMING_AUDIO_CHUNK_QUEUE_SIZE 50 // Queue for audio chunks

// Ring buffer for smooth audio streaming
typedef struct {
    unsigned char *buffer;
    size_t size;
    size_t used;
    size_t read_pos;
    size_t write_pos;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int active;
} AudioRingBuffer;

// Base64 decoding for audio
int decode_base64_audio(const char *base64_input, unsigned char **audio_output, size_t *output_size);

#endif // AUDIO_H 