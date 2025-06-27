#include "input_handler.h"
#include "config.h"
#include "message_queue.h"
#include "utils.h"
#include "websocket_client.h"
#include "audio.h"
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
            if (start_recording()) {
                printf("🎤 Recording started! Say something and type 'stop' to end recording.\n");
            } else {
                printf("❌ Failed to start recording\n");
            }
            printf("> ");
            fflush(stdout);
            continue;
        }
        
        if (strcmp(input_buffer, "stop") == 0) {
            if (stop_recording()) {
                printf("⏹️  Recording stopped. Processing audio...\n");
                
                // Get recorded audio and send it
                unsigned char *audio_data;
                size_t audio_size;
                if (get_recorded_audio(&audio_data, &audio_size)) {
                    // Encode to base64
                    char *base64_audio;
                    if (encode_audio_to_base64(audio_data, audio_size, &base64_audio)) {
                        // Create JSON message with audio
                        char json_message[4096];
                        snprintf(json_message, sizeof(json_message), 
                                "{\"type\":\"audio\",\"audio\":\"%s\"}", base64_audio);
                        
                        // Add to message queue
                        if (add_message_to_queue(json_message)) {
                            get_timestamp(timestamp, sizeof(timestamp));
                            printf("[%s] You: [AUDIO MESSAGE] (%zu bytes)\n", timestamp, audio_size);
                            
                            // Trigger WebSocket writeable callback
                            if (websocket_connection) {
                                lws_callback_on_writable(websocket_connection);
                                lws_service(websocket_context, 0);
                            }
                        }
                        
                        free(base64_audio);
                    }
                    free(audio_data);
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
            printf("🔍 DEBUG: Message added to queue successfully\n");
            printf("> ");
            fflush(stdout);
            
            // Trigger WebSocket writeable callback to send the message
            if (websocket_connection) {
                printf("🔍 DEBUG: Triggering WebSocket writeable callback\n");
                lws_callback_on_writable(websocket_connection);
                
                // Also try to service the context to process the callback
                printf("🔍 DEBUG: Servicing WebSocket context\n");
                lws_service(websocket_context, 0);
            } else {
                printf("❌ DEBUG: WebSocket connection is NULL!\n");
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