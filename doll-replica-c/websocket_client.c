#include "websocket_client.h"
#include "config.h"
#include "utils.h"
#include "message_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Global WebSocket variables
struct lws *websocket_connection = NULL;
struct lws_context *websocket_context = NULL;
int should_exit = 0;

// Timer structure for ping messages
static lws_sorted_usec_list_t ping_timer;

// Function to send ping message
static void send_ping(lws_sorted_usec_list_t *timer) {
    if (!websocket_connection || should_exit) {
        return;
    }

    // Create ping message with timestamp
    time_t current_time = time(NULL);
    char ping_message[64];
    snprintf(ping_message, sizeof(ping_message), "ping-%ld", current_time);
    
    // Prepare ping buffer
    unsigned char ping_buffer[LWS_PRE + 64];
    int message_length = strlen(ping_message);
    memcpy(&ping_buffer[LWS_PRE], ping_message, message_length);
    
    // Send ping
    int result = lws_write(websocket_connection, &ping_buffer[LWS_PRE], message_length, LWS_WRITE_PING);
    
    if (result < 0) {
        printf("❌ Failed to send ping (error: %d)\n", result);
        should_exit = 1;
    }
    // Ping sent successfully - no need to display it

    // Schedule next ping in PING_INTERVAL_SECONDS
    if (!should_exit && websocket_connection) {
        lws_sul_schedule(websocket_context, 0, timer, send_ping, PING_INTERVAL_SECONDS * LWS_US_PER_SEC);
    }
}

// WebSocket callback function - handles all WebSocket events
int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            // Add Basic Authentication header
            char credentials[256];
            char encoded_auth[512];
            char auth_header[512];
            
            // Create "username:password" string
            snprintf(credentials, sizeof(credentials), "%s:%s", AUTH_USERNAME, AUTH_PASSWORD);
            
            // Encode to base64
            encode_base64(credentials, encoded_auth);
            
            // Create "Basic <base64>" header
            snprintf(auth_header, sizeof(auth_header), "Basic %s", encoded_auth);
            
            // Add the header to the request
            unsigned char **header_ptr = (unsigned char **)in;
            unsigned char *header_end = (unsigned char *)in + len;
            
            if (lws_add_http_header_by_name(wsi, 
                                          (const unsigned char *)"authorization",
                                          (const unsigned char *)auth_header,
                                          strlen(auth_header), header_ptr, header_end)) {
                printf("❌ Failed to add Authorization header\n");
                return 1;
            }
            break;
        }
        
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("✅ Connected to WebSocket server!\n");
            websocket_connection = wsi;
            
            // Start sending ping messages every PING_INTERVAL_SECONDS
            lws_sul_schedule(websocket_context, 0, &ping_timer, send_ping, PING_INTERVAL_SECONDS * LWS_US_PER_SEC);
            
            // Send initial greeting message
            printf("🔍 DEBUG: Connection established, scheduling writeable callback\n");
            lws_callback_on_writable(wsi);
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            printf("🔍 DEBUG: Writeable callback triggered\n");
            // Send queued messages
            char message_to_send[MAX_MESSAGE_LENGTH];
            int message_length;
            
            if (get_message_from_queue(message_to_send, &message_length)) {
                printf("🔍 DEBUG: Sending message: '%s' (length: %d)\n", message_to_send, message_length);
                
                unsigned char message_buffer[LWS_PRE + MAX_MESSAGE_LENGTH];
                memcpy(&message_buffer[LWS_PRE], message_to_send, message_length);
                
                int result = lws_write(wsi, &message_buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
                if (result >= 0) {
                    printf("✅ DEBUG: Message sent successfully (%d bytes)\n", result);
                } else {
                    printf("❌ Failed to send message (error: %d)\n", result);
                }
                
                // If there are more messages in queue, schedule another write
                if (queue_head != queue_tail) {
                    printf("🔍 DEBUG: More messages in queue, scheduling another write\n");
                    lws_callback_on_writable(wsi);
                }
            } else {
                printf("🔍 DEBUG: No messages in queue to send\n");
            }
            break;
        }
        
        case LWS_CALLBACK_CLIENT_RECEIVE: {
            char timestamp[16];
            get_timestamp(timestamp, sizeof(timestamp));
            printf("[%s] Server: %.*s\n", timestamp, (int)len, (char *)in);
            printf("> ");
            fflush(stdout);
            break;
        }
        
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            // Pong received - connection is healthy, no need to display
            break;
            
        case LWS_CALLBACK_CLOSED:
            printf("🔌 Connection closed\n");
            websocket_connection = NULL;
            should_exit = 1;
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("❌ Connection error occurred\n");
            websocket_connection = NULL;
            should_exit = 1;
            break;
            
        default:
            break;
    }
    return 0;
}

// WebSocket protocol definition
static struct lws_protocols protocols[] = {
    {
        "websocket",           // Protocol name
        websocket_callback,    // Callback function
        0,                     // Per-session data size
        512,                   // Max frame size
        0, NULL, 0             // Additional parameters
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }  // Terminator
};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    printf("\n🛑 Received signal %d, shutting down...\n", signal);
    should_exit = 1;
}

// Initialize WebSocket client
int init_websocket_client(void) {
    // Enable libwebsockets logging
    lws_set_log_level(LOG_LEVELS, NULL);
    
    // Create WebSocket context
    struct lws_context_creation_info context_info;
    memset(&context_info, 0, sizeof(context_info));
    
    context_info.port = CONTEXT_PORT_NO_LISTEN;  // Client mode
    context_info.protocols = protocols;
    context_info.gid = -1;
    context_info.uid = -1;
    
    websocket_context = lws_create_context(&context_info);
    if (!websocket_context) {
        printf("❌ Failed to create WebSocket context\n");
        return 0;
    }
    
    return 1;
}

// Cleanup WebSocket client
void cleanup_websocket_client(void) {
    if (websocket_context) {
        lws_context_destroy(websocket_context);
        websocket_context = NULL;
    }
}

// Connect to server
int connect_to_server(void) {
    // Set up connection information
    struct lws_client_connect_info connection_info;
    memset(&connection_info, 0, sizeof(connection_info));
    
    connection_info.context = websocket_context;
    connection_info.address = SERVER_ADDRESS;
    connection_info.port = SERVER_PORT;
    connection_info.path = WEBSOCKET_PATH;
    connection_info.host = SERVER_ADDRESS;
    connection_info.origin = SERVER_ADDRESS;
    connection_info.protocol = protocols[0].name;
    connection_info.ssl_connection = 0;  // No SSL for local testing
    
    // Attempt to connect
    if (!lws_client_connect_via_info(&connection_info)) {
        printf("❌ Failed to initiate connection\n");
        return 0;
    }
    
    return 1;
}

// Disconnect from server
void disconnect_from_server(void) {
    if (websocket_connection) {
        lws_callback_on_writable(websocket_connection);
        websocket_connection = NULL;
    }
} 