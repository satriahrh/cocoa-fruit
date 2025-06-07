#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int interrupted;
static struct lws *client_wsi = NULL;
static struct lws_context *global_context = NULL;
static int ping_interval = 1; // Send ping every 1 seconds

// Timer callback for sending pings
static void ping_timer_callback(lws_sorted_usec_list_t *sul) {
    if (!client_wsi || interrupted) {
        return;
    }

    // Send ping directly
    time_t current_time = time(NULL);
    unsigned char ping_payload[LWS_PRE + 32];
    char ping_data[32];
    
    // Create ping payload with timestamp
    snprintf(ping_data, sizeof(ping_data), "ping-%ld", current_time);
    memcpy(&ping_payload[LWS_PRE], ping_data, strlen(ping_data));
    
    int result = lws_write(client_wsi, &ping_payload[LWS_PRE], strlen(ping_data), LWS_WRITE_PING);
    if (result < 0) {
        lwsl_err("Failed to send ping (error: %d)\n", result);
        interrupted = 1; // Exit on ping failure
    } else {
        lwsl_user("✓ Ping sent successfully (payload: %s, bytes: %d)\n", ping_data, result);
    }

    // Schedule next ping (equivalent to Go's ticker)
    if (!interrupted && client_wsi) {
        lws_sul_schedule(global_context, 0, sul, ping_timer_callback, ping_interval * LWS_US_PER_SEC);
    }
}

static lws_sorted_usec_list_t ping_sul;

// Configuration for Basic Auth - modify these values
static const char* auth_username = "John";
static const char* auth_password = "Doe";

// Function to send immediate ping
static void send_ping_now(void) {
    if (client_wsi) {
        lws_callback_on_writable(client_wsi);
        lwsl_user("Manual ping triggered\n");
    }
}

// Base64 encoding function
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const char *input, char *output) {
    int len = strlen(input);
    int i, j = 0;
    
    for (i = 0; i < len; i += 3) {
        unsigned char a = input[i];
        unsigned char b = (i + 1 < len) ? input[i + 1] : 0;
        unsigned char c = (i + 2 < len) ? input[i + 2] : 0;
        
        output[j++] = base64_chars[a >> 2];
        output[j++] = base64_chars[((a & 3) << 4) | (b >> 4)];
        output[j++] = (i + 1 < len) ? base64_chars[((b & 15) << 2) | (c >> 6)] : '=';
        output[j++] = (i + 2 < len) ? base64_chars[c & 63] : '=';
    }
    output[j] = '\0';
}
static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            // Add Basic Auth header
            char credentials[256];
            char encoded_auth[512];
            char auth_value[512];
            
            snprintf(credentials, sizeof(credentials), "%s:%s", auth_username, auth_password);
            base64_encode(credentials, encoded_auth);
            snprintf(auth_value, sizeof(auth_value), "Basic %s", encoded_auth);
            
            unsigned char **p = (unsigned char **)in;
            unsigned char *end = (unsigned char *)in + len;
            
            if (lws_add_http_header_by_name(wsi, 
                                          (const unsigned char *)"authorization",
                                          (const unsigned char *)auth_value,
                                          strlen(auth_value), p, end)) {
                lwsl_err("Failed to add Authorization header\n");
                return 1;
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("✓ Connected to server, initializing...\n");
            client_wsi = wsi;
            lwsl_user("Ping interval: %d seconds\n", ping_interval);
            
            // Start the ping timer (equivalent to Go's ticker)
            lws_sul_schedule(global_context, 0, &ping_sul, ping_timer_callback, ping_interval * LWS_US_PER_SEC);
            
            lws_callback_on_writable(wsi);
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE: {
            // Send regular message only once at the beginning
            static int message_sent = 0;
            if (!message_sent) {
                const char *msg = "Hello from C WebSocket client!";
                unsigned char buf[LWS_PRE + 512];
                size_t n = sprintf((char *)&buf[LWS_PRE], "%s", msg);
                int result = lws_write(wsi, &buf[LWS_PRE], n, LWS_WRITE_TEXT);
                if (result >= 0) {
                    lwsl_user("Initial message sent successfully\n");
                } else {
                    lwsl_err("Failed to send initial message\n");
                }
                message_sent = 1;
            }
            break;
        }
        case LWS_CALLBACK_CLIENT_RECEIVE:
            lwsl_user("Received: %s\n", (char *)in);
            // Don't exit immediately, keep connection alive for pings
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            lwsl_user("✓ Received pong from server (payload: %.*s, length: %d)\n", (int)len, (char *)in, (int)len);
            break;
        case LWS_CALLBACK_CLOSED:
            lwsl_user("Connection closed\n");
            client_wsi = NULL;
            interrupted = 1;
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_user("Connection error\n");
            client_wsi = NULL;
            interrupted = 1;
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {"ws", callback_ws, 0, 512, 0, NULL, 0},
    {NULL, NULL, 0, 0, 0, NULL, 0}
};

static void sigint_handler(int sig) {
    interrupted = 1;
}

int main(void) {
    struct lws_context_creation_info info;
    struct lws_client_connect_info i;
    struct lws_context *context;
    // Enable libwebsockets logging - this is crucial for seeing lwsl_user messages
    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
    lws_set_log_level(logs, NULL);
    
    memset(&info, 0, sizeof info);

    signal(SIGINT, sigint_handler);

    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }
    
    global_context = context; // Store global reference for timer
    
    memset(&i, 0, sizeof i);
    i.context = context;
    i.address = "127.0.0.1"; // You can use wss://echo.websocket.org or a local echo server
    i.port = 8080;
    i.path = "/ws";
    i.host = i.address;
    i.origin = i.address;
    // i.ssl_connection = LCCSCF_USE_SSL; // Use 0 for ws:// (no SSL)
    i.protocol = protocols[0].name;

    if (!lws_client_connect_via_info(&i)) {
        lwsl_err("Client connection failed\n");
        lws_context_destroy(context);
        return 1;
    }
    
    while (!interrupted) {
        lws_service(context, 1000);
    }

    lws_context_destroy(context);
    return 0;
}