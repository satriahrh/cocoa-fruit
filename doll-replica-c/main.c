#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Global variables for connection management
static int should_exit = 0;
static struct lws *websocket_connection = NULL;
static struct lws_context *websocket_context = NULL;

// Configuration
static const char* SERVER_ADDRESS = "127.0.0.1";
static const int SERVER_PORT = 8080;
static const char* WEBSOCKET_PATH = "/ws";
static const char* AUTH_USERNAME = "John";
static const char* AUTH_PASSWORD = "Doe";

// Base64 encoding table
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Function to encode string to base64 (for Basic Auth)
static void encode_base64(const char *input, char *output) {
    int input_length = strlen(input);
    int i, j = 0;
    
    for (i = 0; i < input_length; i += 3) {
        unsigned char byte1 = input[i];
        unsigned char byte2 = (i + 1 < input_length) ? input[i + 1] : 0;
        unsigned char byte3 = (i + 2 < input_length) ? input[i + 2] : 0;
        
        output[j++] = base64_chars[byte1 >> 2];
        output[j++] = base64_chars[((byte1 & 3) << 4) | (byte2 >> 4)];
        output[j++] = (i + 1 < input_length) ? base64_chars[((byte2 & 15) << 2) | (byte3 >> 6)] : '=';
        output[j++] = (i + 2 < input_length) ? base64_chars[byte3 & 63] : '=';
    }
    output[j] = '\0';
}

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
    } else {
        printf("✅ Ping sent: %s (%d bytes)\n", ping_message, result);
    }

    // Schedule next ping in 5 seconds (same as server)
    if (!should_exit && websocket_connection) {
        lws_sul_schedule(websocket_context, 0, timer, send_ping, 5 * LWS_US_PER_SEC);
    }
}

// WebSocket callback function - handles all WebSocket events
static int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
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
            
            // Start sending ping messages every 5 seconds
            lws_sul_schedule(websocket_context, 0, &ping_timer, send_ping, 5 * LWS_US_PER_SEC);
            
            // Send initial greeting message
            lws_callback_on_writable(wsi);
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            // Send initial greeting message
            static int greeting_sent = 0;
            if (!greeting_sent) {
                const char *greeting = "Hello from C WebSocket client!";
                unsigned char message_buffer[LWS_PRE + 512];
                int message_length = sprintf((char *)&message_buffer[LWS_PRE], "%s", greeting);
                
                int result = lws_write(wsi, &message_buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
                if (result >= 0) {
                    printf("✅ Greeting message sent: %s\n", greeting);
                } else {
                    printf("❌ Failed to send greeting message\n");
                }
                greeting_sent = 1;
            }
            break;
        }
        
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("📨 Received message: %.*s\n", (int)len, (char *)in);
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            printf("🏓 Received pong: %.*s\n", (int)len, (char *)in);
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
static void signal_handler(int signal) {
    printf("\n🛑 Received signal %d, shutting down...\n", signal);
    should_exit = 1;
}

int main(void) {
    printf("🚀 Starting C WebSocket client...\n");
    printf("📍 Connecting to: %s:%d%s\n", SERVER_ADDRESS, SERVER_PORT, WEBSOCKET_PATH);
    printf("👤 Username: %s\n", AUTH_USERNAME);
    
    // Set up signal handling for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Enable libwebsockets logging
    int log_levels = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
    lws_set_log_level(log_levels, NULL);
    
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
        return 1;
    }
    
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
        lws_context_destroy(websocket_context);
        return 1;
    }
    
    printf("⏳ Connecting to server...\n");
    
    // Main event loop
    while (!should_exit) {
        lws_service(websocket_context, 1000);  // 1 second timeout
    }
    
    printf("👋 Shutting down WebSocket client\n");
    lws_context_destroy(websocket_context);
    
    return 0;
}