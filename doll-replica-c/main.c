#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include our modular headers
#include "config.h"
#include "websocket_client.h"
#include "message_queue.h"
#include "input_handler.h"

int main(void) {
    printf("🚀 Starting C WebSocket client...\n");
    printf("📍 Connecting to: %s:%d%s\n", SERVER_ADDRESS, SERVER_PORT, WEBSOCKET_PATH);
    printf("👤 Username: %s\n", AUTH_USERNAME);
    
    // Set up signal handling for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize message queue
    init_message_queue();
    
    // Initialize WebSocket client
    if (!init_websocket_client()) {
        printf("❌ Failed to initialize WebSocket client\n");
        cleanup_message_queue();
        return 1;
    }
    
    // Connect to server
    if (!connect_to_server()) {
        printf("❌ Failed to connect to server\n");
        cleanup_websocket_client();
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
        cleanup_message_queue();
        return 1;
    }
    
    printf("✅ Connected! Starting interactive chat...\n");
    
    // Start input thread
    if (!start_input_thread()) {
        printf("❌ Failed to start input thread\n");
        cleanup_websocket_client();
        cleanup_message_queue();
        return 1;
    }
    
    // Main event loop
    while (!should_exit) {
        lws_service(websocket_context, 100);  // 100ms timeout for more responsive input
    }
    
    // Cleanup
    stop_input_thread();
    wait_for_input_thread();
    disconnect_from_server();
    
    printf("\n👋 Shutting down WebSocket client\n");
    
    // Final cleanup
    cleanup_websocket_client();
    cleanup_message_queue();
    
    return 0;
}