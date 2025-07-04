#include "audio.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        size_t bytes_to_write = framesPerBuffer * CHANNELS * 2; // 2 bytes per sample
        
        // If streaming callback is set, send chunk immediately
        if (streaming_callback) {
            streaming_callback((const unsigned char *)inputBuffer, bytes_to_write);
        }
        
        // Also buffer for traditional recording
        if (recording_buffer_used + bytes_to_write <= max_recording_size) {
            memcpy(recording_buffer + recording_buffer_used, inputBuffer, bytes_to_write);
            recording_buffer_used += bytes_to_write;
            
            // Debug: log every 1000 frames (about every 2 seconds at 16kHz)
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 1000 == 0) {
                printf("🔍 DEBUG: Recording callback called, captured %zu bytes so far\n", recording_buffer_used);
            }
        } else {
            // Buffer full, but don't stop recording - just stop capturing
            printf("🔍 DEBUG: Recording buffer full (%zu bytes), stopping capture but keeping stream active\n", recording_buffer_used);
            // Don't set recording_active = 0 here, just stop capturing
        }
    } else {
        printf("🔍 DEBUG: Recording callback called but inputBuffer=%p, recording_active=%d\n", 
               inputBuffer, recording_active);
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
    
    printf("🔊 Playing audio (%zu bytes)...\n", data_size);
    
    // Properly parse WAV file to find PCM data
    size_t pcm_offset = 0;
    if (data_size > 12 && 
        audio_data[0] == 'R' && audio_data[1] == 'I' && 
        audio_data[2] == 'F' && audio_data[3] == 'F') {
        
        printf("🔍 Found WAV file, searching for PCM data...\n");
        
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
                printf("🔍 Found PCM data at offset %zu (skipped %zu bytes)\n", pcm_offset, pcm_offset);
                break;
            }
            
            // Skip to next chunk
            if (pcm_offset + 4 < data_size) {
                uint32_t chunk_size = (audio_data[pcm_offset + 4] << 0) | 
                                    (audio_data[pcm_offset + 5] << 8) | 
                                    (audio_data[pcm_offset + 6] << 16) | 
                                    (audio_data[pcm_offset + 7] << 24);
                pcm_offset += 8 + chunk_size;
                printf("🔍 Skipping chunk, size: %u bytes\n", chunk_size);
            } else {
                break;
            }
        }
        
        // Update audio data pointer and size
        audio_data += pcm_offset;
        data_size -= pcm_offset;
        printf("🔍 PCM data size: %zu bytes\n", data_size);
    }
    
    // Print first 16 bytes of actual PCM data for debugging
    printf("🔍 First 16 bytes of PCM data: ");
    for (int i = 0; i < 16 && i < data_size; i++) {
        printf("%02X ", audio_data[i]);
    }
    printf("\n");
    
    // Ensure data size is a multiple of 2 (16-bit samples)
    if (data_size % 2 != 0) {
        printf("⚠️  Audio data size not multiple of 2, trimming last byte\n");
        data_size--;
    }
    
    // Restart the stream before playback
    Pa_StopStream(audio_stream);
    Pa_StartStream(audio_stream);
    
    // Write audio data in chunks to avoid buffer underflow
    size_t chunk_size = FRAMES_PER_BUFFER * CHANNELS * 2; // 2 bytes per sample
    size_t offset = 0;
    int chunks_written = 0;
    
    printf("🔍 Chunk size: %zu bytes, Total chunks: %zu\n", chunk_size, (data_size + chunk_size - 1) / chunk_size);
    
    while (offset < data_size) {
        size_t bytes_to_write = (offset + chunk_size > data_size) ? 
                               (data_size - offset) : chunk_size;
        
        printf("🔍 Writing chunk %d: %zu bytes (offset: %zu)\n", chunks_written + 1, bytes_to_write, offset);
        
        PaError err = Pa_WriteStream(audio_stream, audio_data + offset, bytes_to_write / 2);
        if (err != paNoError) {
            printf("❌ Failed to play audio chunk %d: %s\n", chunks_written + 1, Pa_GetErrorText(err));
            return 0;
        }
        
        offset += bytes_to_write;
        chunks_written++;
        
        // Small delay to allow audio to play
        usleep(10000); // 10ms
    }
    
    printf("🔍 Wrote %d chunks successfully\n", chunks_written);
    
    // Wait for audio to finish playing
    printf("🔍 Waiting for audio to finish...\n");
    while (Pa_GetStreamWriteAvailable(audio_stream) < FRAMES_PER_BUFFER) {
        usleep(1000); // 1ms
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
    printf("🔍 Base64 input length: %zu\n", strlen(base64_audio));
    
    if (!decode_base64_audio(base64_audio, &audio_data, &audio_size)) {
        printf("❌ Failed to decode base64 audio\n");
        return 0;
    }
    
    printf("🔍 Decoded audio size: %zu bytes\n", audio_size);
    
    int result = play_audio_data(audio_data, audio_size);
    free(audio_data);
    
    return result;
} 