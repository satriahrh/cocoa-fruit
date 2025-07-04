#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include our modular headers
#include "config.h"
#include "websocket_client.h"
#include "message_queue.h"
#include "input_handler.h"
#include "audio.h"
#include "http_client.h"

// Audio chunk streaming callback
void handle_audio_chunk(const unsigned char *chunk, size_t chunk_size) {
    static int chunk_count = 0;
    chunk_count++;
    
    // Stream the audio chunk immediately
    if (!http_stream_audio_chunk(chunk, chunk_size)) {
        printf("❌ Failed to stream audio chunk %d (%zu bytes)\n", chunk_count, chunk_size);
    } else {
        // Log every 10th chunk to avoid spam
        if (chunk_count % 10 == 0) {
            printf("📤 Streamed audio chunk %d (%zu bytes)\n", chunk_count, chunk_size);
        }
    }
}

int main(void) {
    printf("🚀 Starting C WebSocket client with real-time HTTP audio streaming...\n");
    printf("📍 Connecting to: %s:%d%s\n", SERVER_ADDRESS, SERVER_PORT, WEBSOCKET_PATH);
    printf("🌐 HTTP Server: %s:%d\n", HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT);
    
    // Set up signal handling for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize message queue
    init_message_queue();
    
    // Initialize audio system
    if (!init_audio()) {
        printf("❌ Failed to initialize audio system\n");
        cleanup_message_queue();
        return 1;
    }
    
    // Initialize HTTP client for audio streaming
    if (!http_init()) {
        printf("❌ Failed to initialize HTTP client\n");
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    // Get JWT token for HTTP authentication
    if (!http_get_jwt_token(current_jwt_token, sizeof(current_jwt_token))) {
        printf("❌ Failed to get JWT token\n");
        http_cleanup();
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    // Initialize WebSocket client (for responses only)
    if (!init_websocket_client()) {
        printf("❌ Failed to initialize WebSocket client\n");
        http_cleanup();
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    // Connect to server
    if (!connect_to_server()) {
        printf("❌ Failed to connect to server\n");
        cleanup_websocket_client();
        http_cleanup();
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    printf("⏳ Connecting to server...\n");
    
    // Wait a moment for connection to establish
    int connection_attempts = 0;
    while (!websocket_connection && connection_attempts < 10 && !should_exit) {
        lws_service(websocket_context, 1000);
        connection_attempts++;
    }
    
    if (!websocket_connection) {
        printf("❌ Failed to connect to server\n");
        cleanup_websocket_client();
        http_cleanup();
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    printf("✅ Connected! Starting interactive chat with real-time HTTP audio streaming...\n");
    printf("🎤 Audio will be streamed in real-time during recording\n");
    printf("💬 Text responses will come via WebSocket\n");
    
    // Start input thread
    if (!start_input_thread()) {
        printf("❌ Failed to start input thread\n");
        cleanup_websocket_client();
        http_cleanup();
        cleanup_audio();
        cleanup_message_queue();
        return 1;
    }
    
    // Main event loop
    while (!should_exit) {
        lws_service(websocket_context, 10);  // 10ms timeout for more responsive input
    }
    
    // Cleanup
    stop_input_thread();
    wait_for_input_thread();
    disconnect_from_server();
    
    printf("\n👋 Shutting down client\n");
    
    // Final cleanup
    cleanup_websocket_client();
    http_cleanup();
    cleanup_message_queue();
    cleanup_audio();
    
    return 0;
}