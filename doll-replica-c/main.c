#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>

// Global variables for connection management
static int should_exit = 0;
static struct lws *websocket_connection = NULL;
static struct lws_context *websocket_context = NULL;

// Message queue for sending messages
#define MAX_MESSAGE_LENGTH 1024
#define MAX_QUEUE_SIZE 10

typedef struct {
    char message[MAX_MESSAGE_LENGTH];
    int length;
} queued_message_t;

static queued_message_t message_queue[MAX_QUEUE_SIZE];
static int queue_head = 0;
static int queue_tail = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

// Configuration
static const char* SERVER_ADDRESS = "127.0.0.1";
static const int SERVER_PORT = 8080;
static const char* WEBSOCKET_PATH = "/ws";
static const char* AUTH_USERNAME = "John";
static const char* AUTH_PASSWORD = "Doe";
static const int PING_INTERVAL_SECONDS = 30;  // More IoT-friendly

// Base64 encoding table
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Helper function to get current timestamp string
static void get_timestamp(char *timestamp, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(timestamp, size, "%H:%M:%S", tm_info);
}

// Helper function to add message to queue
static int add_message_to_queue(const char *message) {
    pthread_mutex_lock(&queue_mutex);
    
    int next_tail = (queue_tail + 1) % MAX_QUEUE_SIZE;
    if (next_tail == queue_head) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is full
    }
    
    strncpy(message_queue[queue_tail].message, message, MAX_MESSAGE_LENGTH - 1);
    message_queue[queue_tail].message[MAX_MESSAGE_LENGTH - 1] = '\0';
    message_queue[queue_tail].length = strlen(message_queue[queue_tail].message);
    queue_tail = next_tail;
    
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
}

// Helper function to get message from queue
static int get_message_from_queue(char *message, int *length) {
    pthread_mutex_lock(&queue_mutex);
    
    if (queue_head == queue_tail) {
        pthread_mutex_unlock(&queue_mutex);
        return 0; // Queue is empty
    }
    
    strcpy(message, message_queue[queue_head].message);
    *length = message_queue[queue_head].length;
    queue_head = (queue_head + 1) % MAX_QUEUE_SIZE;
    
    pthread_mutex_unlock(&queue_mutex);
    return 1; // Success
}

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
    }
    // Ping sent successfully - no need to display it

    // Schedule next ping in PING_INTERVAL_SECONDS seconds (same as server)
    if (!should_exit && websocket_connection) {
        lws_sul_schedule(websocket_context, 0, timer, send_ping, PING_INTERVAL_SECONDS * LWS_US_PER_SEC);
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
            
            // Start sending ping messages every PING_INTERVAL_SECONDS seconds
            lws_sul_schedule(websocket_context, 0, &ping_timer, send_ping, PING_INTERVAL_SECONDS * LWS_US_PER_SEC);
            
            // Send initial greeting message
            lws_callback_on_writable(wsi);
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            // Send queued messages
            char message_to_send[MAX_MESSAGE_LENGTH];
            int message_length;
            
            if (get_message_from_queue(message_to_send, &message_length)) {
                unsigned char message_buffer[LWS_PRE + MAX_MESSAGE_LENGTH];
                memcpy(&message_buffer[LWS_PRE], message_to_send, message_length);
                
                int result = lws_write(wsi, &message_buffer[LWS_PRE], message_length, LWS_WRITE_TEXT);
                if (result >= 0) {
                    // Message sent successfully
                } else {
                    printf("❌ Failed to send message\n");
                }
                
                // If there are more messages in queue, schedule another write
                if (queue_head != queue_tail) {
                    lws_callback_on_writable(wsi);
                }
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
static void signal_handler(int signal) {
    printf("\n🛑 Received signal %d, shutting down...\n", signal);
    should_exit = 1;
}

// Input thread function - reads keyboard input
static void* input_thread(void *arg) {
    char input_buffer[MAX_MESSAGE_LENGTH];
    char timestamp[16];
    
    printf("\n💬 Type your message and press Enter to send (Ctrl+C to exit):\n");
    printf("> ");
    fflush(stdout);
    
    while (!should_exit) {
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
        
        // Add message to queue
        if (add_message_to_queue(input_buffer)) {
            get_timestamp(timestamp, sizeof(timestamp));
            printf("[%s] You: %s\n", timestamp, input_buffer);
            printf("> ");
            fflush(stdout);
            
            // Trigger WebSocket writeable callback to send the message
            if (websocket_connection) {
                lws_callback_on_writable(websocket_connection);
            }
        } else {
            printf("❌ Message queue is full, please wait...\n> ");
            fflush(stdout);
        }
    }
    
    return NULL;
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
    
    // Wait a moment for connection to establish
    int connection_attempts = 0;
    while (!websocket_connection && connection_attempts < 10 && !should_exit) {
        lws_service(websocket_context, 1000);
        connection_attempts++;
    }
    
    if (!websocket_connection) {
        printf("❌ Failed to connect to server\n");
        lws_context_destroy(websocket_context);
        return 1;
    }
    
    printf("✅ Connected! Starting interactive chat...\n");
    
    // Start input thread
    pthread_t input_tid;
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        printf("❌ Failed to create input thread\n");
        lws_context_destroy(websocket_context);
        return 1;
    }
    
    // Main event loop
    while (!should_exit) {
        lws_service(websocket_context, 100);  // 100ms timeout for more responsive input
    }
    
    // Wait for input thread to finish
    pthread_join(input_tid, NULL);
    
    printf("\n👋 Shutting down WebSocket client\n");
    lws_context_destroy(websocket_context);
    
    return 0;
}