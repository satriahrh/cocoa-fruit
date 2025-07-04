#include "input_handler.h"
#include "config.h"
#include "message_queue.h"
#include "utils.h"
#include "websocket_client.h"
#include "audio.h"
#include "http_client.h"
#include <stdio.h>
#include <string.h>

static pthread_t input_tid;
static int input_thread_running = 0;

// Input thread function - reads keyboard input
void* input_thread(void *arg) {
    char input_buffer[MAX_MESSAGE_LENGTH];
    char timestamp[16];
    
    printf("\n💬 Type your message and press Enter to send (Ctrl+C to exit):\n");
    printf("🎤 Commands: 'record' to start recording, 'stop' to stop recording\n");
    printf("> ");
    fflush(stdout);
    
    while (!should_exit && input_thread_running) {
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            if (feof(stdin)) {
                printf("\nEOF detected, exiting...\n");
                should_exit = 1;
                break;
            }
            continue;
        }
        
        // Remove newline character
        size_t len = strlen(input_buffer);
        if (len > 0 && input_buffer[len-1] == '\n') {
            input_buffer[len-1] = '\0';
            len--;
        }
        
        // Skip empty messages
        if (len == 0) {
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Handle recording commands
        if (strcmp(input_buffer, "record") == 0) {
            // Start recording without streaming (collect all audio first)
            if (start_recording()) {
                printf("🎤 Recording started! Say something and type 'stop' to end recording.\n");
                printf("📤 Audio will be sent as a single request when recording stops...\n");
            } else {
                printf("❌ Failed to start recording\n");
            }
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        if (strcmp(input_buffer, "stop") == 0) {
            if (stop_recording()) {
                printf("⏹️  Recording stopped.\n");
                
                // Get the recorded audio data
                unsigned char *audio_data = NULL;
                size_t audio_size = 0;
                
                if (get_recorded_audio(&audio_data, &audio_size)) {
                    printf("📤 Sending %zu bytes of audio data...\n", audio_size);
                    
                    // Send audio data as a single request
                    if (http_stream_audio_realtime(current_jwt_token, audio_data, audio_size)) {
                        printf("✅ Audio sent successfully! Transcription will be sent via WebSocket.\n");
                    } else {
                        printf("❌ Failed to send audio data\n");
                    }
                    
                    // Free the audio data
                    free(audio_data);
                } else {
                    printf("❌ Failed to get recorded audio data\n");
                }
            } else {
                printf("❌ Failed to stop recording\n");
            }
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        // Regular text message handling
        if (add_message_to_queue(input_buffer)) {
            get_timestamp(timestamp, sizeof(timestamp));
            printf("[%s] You: %s\n", timestamp, input_buffer);
            printf("> ");
            fflush(stdout);
            
            // Trigger WebSocket writeable callback to send the message
            if (websocket_connection) {
                lws_callback_on_writable(websocket_connection);
                
                // Also try to service the context to process the callback
                lws_service(websocket_context, 0);
                // Additional service call to ensure immediate processing
                lws_service(websocket_context, 50);
            } else {
                printf("❌ WebSocket connection is NULL!\n");
            }
        } else {
            printf("❌ Message queue is full, please wait...\n> ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

// Start input thread
int start_input_thread(void) {
    input_thread_running = 1;
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        printf("❌ Failed to create input thread\n");
        return 0;
    }
    return 1;
}

// Stop input thread
void stop_input_thread(void) {
    input_thread_running = 0;
}

// Wait for input thread to finish
void wait_for_input_thread(void) {
    pthread_join(input_tid, NULL);
} 