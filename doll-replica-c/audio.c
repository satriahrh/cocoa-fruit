#include "audio.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static PaStream *audio_stream = NULL;
static PaStream *recording_stream = NULL;
static int audio_initialized = 0;
static int recording_active = 0;

// Recording buffer
static unsigned char *recording_buffer = NULL;
static size_t recording_buffer_size = 0;
static size_t recording_buffer_used = 0;
static size_t max_recording_size;

// Streaming callback
static audio_chunk_callback streaming_callback = NULL;

// Streaming audio playback variables
static int streaming_audio_active = 0;
static unsigned char *streaming_audio_buffer = NULL;
static size_t streaming_audio_buffer_size = 0;
static size_t streaming_audio_buffer_used = 0;
static pthread_mutex_t streaming_audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t streaming_audio_cond = PTHREAD_COND_INITIALIZER;

// Ring buffer for smooth streaming
static AudioRingBuffer *streaming_ring_buffer = NULL;
static PaStream *streaming_stream = NULL;

// Base64 decoding table (same as in utils.c)
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Recording callback function
static int recording_callback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData) {
    (void)outputBuffer; // Unused
    (void)timeInfo;     // Unused
    (void)statusFlags;  // Unused
    (void)userData;     // Unused
    
    if (inputBuffer && recording_active) {
        size_t bytes_to_write = framesPerBuffer * CHANNELS * 1; // 1 byte per sample (MULAW)
        
        // If streaming callback is set, send chunk immediately
        if (streaming_callback) {
            streaming_callback((const unsigned char *)inputBuffer, bytes_to_write);
        }
        
        // Also buffer for traditional recording
        if (recording_buffer_used + bytes_to_write <= max_recording_size) {
            memcpy(recording_buffer + recording_buffer_used, inputBuffer, bytes_to_write);
            recording_buffer_used += bytes_to_write;
            
        } else {
            // Buffer full, but don't stop recording - just stop capturing
            // Don't set recording_active = 0 here, just stop capturing
        }
    }
    
    return paContinue;
}

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
    
    // Calculate maximum recording size
    max_recording_size = SAMPLE_RATE * CHANNELS * 2 * MAX_RECORDING_DURATION; // 2 bytes per sample
    
    // Allocate recording buffer
    recording_buffer = malloc(max_recording_size);
    if (!recording_buffer) {
        printf("❌ Failed to allocate recording buffer\n");
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
    if (recording_active) {
        stop_recording();
    }
    
    if (recording_buffer) {
        free(recording_buffer);
        recording_buffer = NULL;
    }
    
    if (recording_stream) {
        Pa_CloseStream(recording_stream);
        recording_stream = NULL;
    }
    
    if (audio_stream) {
        Pa_StopStream(audio_stream);
        Pa_CloseStream(audio_stream);
        audio_stream = NULL;
    }
    
    // Cleanup streaming
    if (streaming_audio_active) {
        stop_streaming_audio_playback();
    }
    
    if (audio_initialized) {
        Pa_Terminate();
        audio_initialized = 0;
        printf("🔇 Audio system cleaned up\n");
    }
}

// Start recording audio
int start_recording(void) {
    return start_recording_with_streaming(NULL);
}

// Start recording with streaming callback
int start_recording_with_streaming(audio_chunk_callback callback) {
    if (!audio_initialized) {
        printf("❌ Audio system not initialized\n");
        return 0;
    }
    
    if (recording_active) {
        printf("⚠️  Recording already active\n");
        return 0;
    }
    
    PaError err = Pa_OpenDefaultStream(&recording_stream,
                                     CHANNELS,             // Input channels
                                     0,                    // No output channels
                                     AUDIO_FORMAT,         // Sample format
                                     SAMPLE_RATE,          // Sample rate
                                     FRAMES_PER_BUFFER,    // Frames per buffer
                                     recording_callback,   // Recording callback
                                     NULL);                // No user data
    
    if (err != paNoError) {
        printf("❌ Failed to open recording stream: %s\n", Pa_GetErrorText(err));
        return 0;
    }
    
    err = Pa_StartStream(recording_stream);
    if (err != paNoError) {
        printf("❌ Failed to start recording stream: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(recording_stream);
        recording_stream = NULL;
        return 0;
    }
    
    // Reset recording buffer
    recording_buffer_used = 0;
    recording_active = 1;
    streaming_callback = callback;
    
    printf("🎤 Started recording...\n");
    return 1;
}

// Check if recording is active
int is_recording_active(void) {
    return recording_active;
}

// Stop recording audio
int stop_recording(void) {
    if (!recording_active || !recording_stream) {
        printf("⚠️  No active recording\n");
        return 0;
    }
    
    PaError err = Pa_StopStream(recording_stream);
    if (err != paNoError) {
        printf("❌ Failed to stop recording stream: %s\n", Pa_GetErrorText(err));
        return 0;
    }
    
    err = Pa_CloseStream(recording_stream);
    if (err != paNoError) {
        printf("❌ Failed to close recording stream: %s\n", Pa_GetErrorText(err));
        return 0;
    }
    
    recording_stream = NULL;
    recording_active = 0;
    streaming_callback = NULL;
    
    printf("⏹️  Recording stopped (%zu bytes captured)\n", recording_buffer_used);
    return 1;
}

// Get recorded audio data
int get_recorded_audio(unsigned char **audio_data, size_t *data_size) {
    if (recording_active) {
        printf("❌ Cannot get audio while recording is active\n");
        return 0;
    }
    
    if (recording_buffer_used == 0) {
        printf("❌ No recorded audio available\n");
        return 0;
    }
    
    // Allocate memory for the recorded audio
    *audio_data = malloc(recording_buffer_used);
    if (!*audio_data) {
        printf("❌ Failed to allocate memory for recorded audio\n");
        return 0;
    }
    
    // Copy recorded data
    memcpy(*audio_data, recording_buffer, recording_buffer_used);
    *data_size = recording_buffer_used;
    
    return 1;
}

// Encode audio data to base64
int encode_audio_to_base64(const unsigned char *audio_data, size_t data_size, char **base64_output) {
    size_t base64_size = ((data_size + 2) / 3) * 4 + 1; // +1 for null terminator
    *base64_output = malloc(base64_size);
    
    if (!*base64_output) {
        printf("❌ Failed to allocate memory for base64 output\n");
        return 0;
    }
    
    int i, j = 0;
    for (i = 0; i < data_size; i += 3) {
        unsigned char byte1 = audio_data[i];
        unsigned char byte2 = (i + 1 < data_size) ? audio_data[i + 1] : 0;
        unsigned char byte3 = (i + 2 < data_size) ? audio_data[i + 2] : 0;
        
        (*base64_output)[j++] = base64_chars[byte1 >> 2];
        (*base64_output)[j++] = base64_chars[((byte1 & 3) << 4) | (byte2 >> 4)];
        (*base64_output)[j++] = (i + 1 < data_size) ? base64_chars[((byte2 & 15) << 2) | (byte3 >> 6)] : '=';
        (*base64_output)[j++] = (i + 2 < data_size) ? base64_chars[byte3 & 63] : '=';
    }
    
    (*base64_output)[j] = '\0';
    return 1;
}

// Play audio data directly
int play_audio_data(const unsigned char *audio_data, size_t data_size) {
    if (!audio_initialized || !audio_stream) {
        printf("❌ Audio system not initialized\n");
        return 0;
    }
    
    // Properly parse WAV file to find PCM data
    size_t pcm_offset = 0;
    if (data_size > 12 && 
        audio_data[0] == 'R' && audio_data[1] == 'I' && 
        audio_data[2] == 'F' && audio_data[3] == 'F') {
        
        // Skip RIFF header (12 bytes)
        pcm_offset = 12;
        
        // Search for the "data" chunk
        while (pcm_offset + 8 < data_size) {
            if (audio_data[pcm_offset] == 'd' && 
                audio_data[pcm_offset + 1] == 'a' && 
                audio_data[pcm_offset + 2] == 't' && 
                audio_data[pcm_offset + 3] == 'a') {
                
                // Found data chunk, skip chunk header (8 bytes)
                pcm_offset += 8;
                break;
            }
            
            // Skip to next chunk
            if (pcm_offset + 4 < data_size) {
                uint32_t chunk_size = (audio_data[pcm_offset + 4] << 0) | 
                                    (audio_data[pcm_offset + 5] << 8) | 
                                    (audio_data[pcm_offset + 6] << 16) | 
                                    (audio_data[pcm_offset + 7] << 24);
                pcm_offset += 8 + chunk_size;
            } else {
                break;
            }
        }
        
        // Update audio data pointer and size
        audio_data += pcm_offset;
        data_size -= pcm_offset;
    }
    
    // For MULAW (8-bit), data size should be fine as-is
    // No need to check for even/odd since each sample is 1 byte
    
    // Restart the stream before playback
    Pa_StopStream(audio_stream);
    Pa_StartStream(audio_stream);
    
    // Write audio data in chunks to avoid buffer underflow
    size_t chunk_size = FRAMES_PER_BUFFER * CHANNELS * 1; // 1 byte per sample (MULAW)
    size_t offset = 0;
    
    while (offset < data_size) {
        size_t bytes_to_write = (offset + chunk_size > data_size) ? 
                               (data_size - offset) : chunk_size;
        
        PaError err = Pa_WriteStream(audio_stream, audio_data + offset, bytes_to_write);
        if (err != paNoError) {
            printf("❌ Failed to play audio: %s\n", Pa_GetErrorText(err));
            return 0;
        }
        
        offset += bytes_to_write;
        
        // Small delay to allow audio to play
        usleep(10000); // 10ms
    }
    
    // Wait for audio to finish playing
    while (Pa_GetStreamWriteAvailable(audio_stream) < FRAMES_PER_BUFFER) {
        usleep(1000); // 1ms
    }
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
    
    if (!decode_base64_audio(base64_audio, &audio_data, &audio_size)) {
        printf("❌ Failed to decode base64 audio\n");
        return 0;
    }
    
    int result = play_audio_data(audio_data, audio_size);
    free(audio_data);
    
    return result;
}

// ============================================================================
// RING BUFFER FUNCTIONS
// ============================================================================

// Initialize ring buffer
AudioRingBuffer* init_audio_ring_buffer(size_t size) {
    AudioRingBuffer *rb = malloc(sizeof(AudioRingBuffer));
    if (!rb) {
        printf("❌ Failed to allocate ring buffer structure\n");
        return NULL;
    }
    
    rb->buffer = malloc(size);
    if (!rb->buffer) {
        printf("❌ Failed to allocate ring buffer memory\n");
        free(rb);
        return NULL;
    }
    
    rb->size = size;
    rb->used = 0;
    rb->read_pos = 0;
    rb->write_pos = 0;
    rb->active = 1;
    
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    pthread_cond_init(&rb->not_full, NULL);
    
    printf("✅ Ring buffer initialized (%zu bytes)\n", size);
    return rb;
}

// Write to ring buffer
int write_audio_buffer(AudioRingBuffer *rb, const unsigned char *data, size_t size) {
    if (!rb || !rb->active) return 0;
    
    pthread_mutex_lock(&rb->mutex);
    
    // Wait if buffer is full
    while (rb->used + size > rb->size && rb->active) {
        pthread_cond_wait(&rb->not_full, &rb->mutex);
    }
    
    if (!rb->active) {
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }
    
    // Write data to buffer
    size_t first_chunk = rb->size - rb->write_pos;
    if (first_chunk > size) first_chunk = size;
    
    memcpy(rb->buffer + rb->write_pos, data, first_chunk);
    
    if (first_chunk < size) {
        memcpy(rb->buffer, data + first_chunk, size - first_chunk);
    }
    
    rb->write_pos = (rb->write_pos + size) % rb->size;
    rb->used += size;
    
    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->mutex);
    
    return 1;
}

// Read from ring buffer
int read_audio_buffer(AudioRingBuffer *rb, unsigned char *data, size_t size) {
    if (!rb || !rb->active) return 0;
    
    pthread_mutex_lock(&rb->mutex);
    
    // Wait if buffer is empty
    while (rb->used < size && rb->active) {
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }
    
    if (!rb->active) {
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }
    
    // Read data from buffer
    size_t first_chunk = rb->size - rb->read_pos;
    if (first_chunk > size) first_chunk = size;
    
    memcpy(data, rb->buffer + rb->read_pos, first_chunk);
    
    if (first_chunk < size) {
        memcpy(data + first_chunk, rb->buffer, size - first_chunk);
    }
    
    rb->read_pos = (rb->read_pos + size) % rb->size;
    rb->used -= size;
    
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    
    return 1;
}

// Cleanup ring buffer
void cleanup_audio_ring_buffer(AudioRingBuffer *rb) {
    if (!rb) return;
    
    pthread_mutex_lock(&rb->mutex);
    rb->active = 0;
    pthread_cond_signal(&rb->not_empty);
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
    
    if (rb->buffer) {
        free(rb->buffer);
    }
    free(rb);
}

// ============================================================================
// STREAMING AUDIO PLAYBACK FUNCTIONS
// ============================================================================

// Streaming playback callback
static int streaming_playback_callback(const void *inputBuffer, void *outputBuffer,
                                      unsigned long framesPerBuffer,
                                      const PaStreamCallbackTimeInfo *timeInfo,
                                      PaStreamCallbackFlags statusFlags,
                                      void *userData) {
    (void)inputBuffer; // Unused
    (void)timeInfo;    // Unused
    (void)userData;    // Unused
    
    if (statusFlags & paOutputUnderflow) {
        printf("⚠️  Audio underflow detected\n");
    }
    
    if (streaming_ring_buffer && streaming_ring_buffer->active) {
        size_t bytes_needed = framesPerBuffer * CHANNELS * 1; // 1 byte per sample (MULAW)
        unsigned char *temp_buffer = malloc(bytes_needed);
        
        if (read_audio_buffer(streaming_ring_buffer, temp_buffer, bytes_needed)) {
            memcpy(outputBuffer, temp_buffer, bytes_needed);
        } else {
            // Buffer underrun - fill with silence
            memset(outputBuffer, 0, bytes_needed);
        }
        
        free(temp_buffer);
    } else {
        // No data - fill with silence
        size_t bytes_needed = framesPerBuffer * CHANNELS * 1;
        memset(outputBuffer, 0, bytes_needed);
    }
    
    return paContinue;
}

// LINEAR16 (PCM) audio format detection and handling for Google TTS streaming
static int is_linear16_data(const unsigned char *data, size_t size) {
    // LINEAR16 is raw PCM data at 24000 Hz, 16-bit, mono
    // We'll assume any data that's not obviously MP3 is LINEAR16
    if (size < 2) return 0;
    
    // Check for MP3 sync word to exclude MP3 data
    if (data[0] == 0xFF && (data[1] == 0xFB || data[1] == 0xFA)) {
        return 0; // This is MP3
    }
    
    // Check for ID3v2 header to exclude MP3 data
    if (size >= 10 && 
        data[0] == 'I' && data[1] == 'D' && data[2] == '3' &&
        data[3] >= 0x02 && data[3] <= 0x04) {
        return 0; // This is MP3
    }
    
    // Assume it's LINEAR16 PCM data from Google TTS streaming
    return 1;
}

// Handle LINEAR16 PCM audio data (Google TTS streaming format)
static int handle_linear16_audio(const unsigned char *audio_data, size_t audio_size, 
                                unsigned char **pcm_data, size_t *pcm_size) {
    printf("🎵 Processing MULAW audio data (%zu bytes, 8000 Hz)\n", audio_size);
    
    // MULAW data from Google TTS streaming - PortAudio supports MULAW natively
    // Format: 8000 Hz, 8-bit MULAW, mono
    // Just copy the data for playback
    *pcm_data = malloc(audio_size);
    if (!*pcm_data) {
        printf("❌ Failed to allocate memory for PCM data\n");
        return 0;
    }
    
    memcpy(*pcm_data, audio_data, audio_size);
    *pcm_size = audio_size;
    
    printf("✅ MULAW audio data ready for playback (8000 Hz)\n");
    return 1;
}

// Start streaming audio playback
int start_streaming_audio_playback(void) {
    if (!audio_initialized) {
        printf("❌ Audio system not initialized\n");
        return 0;
    }
    
    if (streaming_audio_active) {
        printf("⚠️  Streaming audio already active\n");
        return 0;
    }
    
    // Initialize ring buffer
    streaming_ring_buffer = init_audio_ring_buffer(1024 * 1024); // 1MB buffer
    if (!streaming_ring_buffer) {
        printf("❌ Failed to initialize ring buffer\n");
        return 0;
    }
    
    // Configure output parameters
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = CHANNELS;
    outputParameters.sampleFormat = AUDIO_FORMAT;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    
    // Open streaming stream with callback
    PaError err = Pa_OpenStream(&streaming_stream,
                               NULL,                    // No input
                               &outputParameters,       // Output
                               SAMPLE_RATE,             // Sample rate
                               FRAMES_PER_BUFFER,       // Frames per buffer
                               paClipOff | paDitherOff, // Stream flags
                               streaming_playback_callback, // Callback
                               NULL);                   // User data
    
    if (err != paNoError) {
        printf("❌ Failed to open streaming stream: %s\n", Pa_GetErrorText(err));
        cleanup_audio_ring_buffer(streaming_ring_buffer);
        streaming_ring_buffer = NULL;
        return 0;
    }
    
    // Start the stream
    err = Pa_StartStream(streaming_stream);
    if (err != paNoError) {
        printf("❌ Failed to start streaming stream: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(streaming_stream);
        cleanup_audio_ring_buffer(streaming_ring_buffer);
        streaming_ring_buffer = NULL;
        streaming_stream = NULL;
        return 0;
    }
    
    streaming_audio_active = 1;
    printf("✅ Streaming audio playback started with ring buffer\n");
    return 1;
}

// Stop streaming audio playback
int stop_streaming_audio_playback(void) {
    if (!streaming_audio_active) {
        return 0;
    }
    
    streaming_audio_active = 0;
    
    // Stop and close streaming stream
    if (streaming_stream) {
        Pa_StopStream(streaming_stream);
        Pa_CloseStream(streaming_stream);
        streaming_stream = NULL;
    }
    
    // Cleanup ring buffer
    if (streaming_ring_buffer) {
        cleanup_audio_ring_buffer(streaming_ring_buffer);
        streaming_ring_buffer = NULL;
    }
    
    printf("⏹️  Streaming audio playback stopped\n");
    return 1;
}

// Check if streaming audio is active
int is_streaming_audio_active(void) {
    return streaming_audio_active;
}

// Play an audio chunk (called from WebSocket callback)
int play_audio_chunk(const unsigned char *audio_chunk, size_t chunk_size) {
    if (!streaming_audio_active || !streaming_ring_buffer) {
        printf("❌ Streaming audio not active or ring buffer not initialized\n");
        return 0;
    }
    
    if (!audio_chunk || chunk_size == 0) {
        printf("❌ Invalid audio chunk\n");
        return 0;
    }
    
    printf("🎵 Adding audio chunk to ring buffer (%zu bytes)\n", chunk_size);
    
    // Handle MULAW audio data from Google TTS streaming
    unsigned char *pcm_data;
    size_t pcm_size;
    
    if (!handle_linear16_audio(audio_chunk, chunk_size, &pcm_data, &pcm_size)) {
        printf("❌ Failed to process audio chunk\n");
        return 0;
    }
    
    // Write to ring buffer (non-blocking)
    int result = write_audio_buffer(streaming_ring_buffer, pcm_data, pcm_size);
    
    // Clean up
    free(pcm_data);
    
    if (result) {
        printf("✅ Audio chunk added to ring buffer successfully\n");
    } else {
        printf("❌ Failed to add audio chunk to ring buffer\n");
    }
    
    return result;
} 