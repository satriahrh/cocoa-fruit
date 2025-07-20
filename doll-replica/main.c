// Minimal doll-replica: Connects to a WebSocket, receives PCM audio, and plays it using PortAudio.
// This example uses libwebsockets for WebSocket and PortAudio for playback.
// You must link with -lwebsockets -lportaudio when compiling.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <portaudio.h>
#include <libwebsockets.h>

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define FRAMES_PER_BUFFER 512
#define PCM_BUFFER_SIZE (FRAMES_PER_BUFFER * CHANNELS * 2)


#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080
#define WEBSOCKET_PATH "/ws"

#define HTTP_API_KEY "John"
#define HTTP_API_SECRET "Doe"
#define MAX_JWT_TOKEN_LENGTH 1024

// --- Minimal HTTP POST for JWT token ---
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

static int get_jwt_token(char *jwt_token, size_t max_length) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    struct hostent *server = gethostbyname(SERVER_ADDRESS);
    if (!server) { close(sock); return 0; }
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { close(sock); return 0; }
    char req[1024];
    snprintf(req, sizeof(req),
        "POST /api/v1/auth/token HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "X-API-Key: %s\r\n"
        "X-API-Secret: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n",
        SERVER_ADDRESS, SERVER_PORT, HTTP_API_KEY, HTTP_API_SECRET);
    if (send(sock, req, strlen(req), 0) < 0) { close(sock); return 0; }
    char resp[4096];
    ssize_t n = recv(sock, resp, sizeof(resp)-1, 0);
    close(sock);
    if (n <= 0) return 0;
    resp[n] = 0;
    char *token_start = strstr(resp, "\"token\":\"");
    if (token_start) {
        token_start += 9;
        char *token_end = strchr(token_start, '"');
        if (token_end) {
            size_t len = token_end - token_start;
            if (len < max_length) {
                strncpy(jwt_token, token_start, len);
                jwt_token[len] = 0;
                return 1;
            }
        }
    }
    return 0;
}

static PaStream *stream = NULL;

// PortAudio playback callback (not used, we use blocking write)

// WebSocket receive callback
static char jwt_token[MAX_JWT_TOKEN_LENGTH] = {0};
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_RECEIVE:
            // Assume binary PCM 16kHz mono 16-bit
            if (stream && len > 0) {
                Pa_WriteStream(stream, in, len / 2); // 2 bytes/sample
            }
            break;
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            // Add JWT Authentication header
            char auth_header[1024];
            snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt_token);
            unsigned char **p = (unsigned char **)in;
            unsigned char *end = (unsigned char *)in + len;
            if (lws_add_http_header_by_name(wsi,
                    (const unsigned char *)"authorization",
                    (const unsigned char *)auth_header,
                    strlen(auth_header), p, end)) {
                printf("❌ Failed to add Authorization header\n");
                return 1;
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    { "audio-protocol", ws_callback, 0, PCM_BUFFER_SIZE },
    { NULL, NULL, 0, 0 }
};

int main(int argc, char **argv) {
    // Get JWT token first
    if (!get_jwt_token(jwt_token, sizeof(jwt_token))) {
        printf("Failed to get JWT token\n");
        return 1;
    }
    printf("JWT token: %.20s...\n", jwt_token);
    // Initialize PortAudio
    if (Pa_Initialize() != paNoError) {
        printf("Failed to initialize PortAudio\n");
        return 1;
    }
    if (Pa_OpenDefaultStream(&stream, 0, CHANNELS, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, NULL, NULL) != paNoError) {
        printf("Failed to open PortAudio stream\n");
        Pa_Terminate();
        return 1;
    }
    if (Pa_StartStream(stream) != paNoError) {
        printf("Failed to start PortAudio stream\n");
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    // Setup libwebsockets context
    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        printf("Failed to create lws context\n");
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }


    // Use config.h macros for address, port, and path
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = context;
    ccinfo.address = SERVER_ADDRESS;
    ccinfo.port = SERVER_PORT;
    ccinfo.path = WEBSOCKET_PATH;
    ccinfo.host = SERVER_ADDRESS;
    ccinfo.origin = SERVER_ADDRESS;
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = 0;
    // The rest can remain NULL/0

    struct lws *wsi = lws_client_connect_via_info(&ccinfo);
    if (!wsi) {
        printf("Failed to connect to WebSocket (%s:%d%s)\n", SERVER_ADDRESS, SERVER_PORT, WEBSOCKET_PATH);
        lws_context_destroy(context);
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    printf("Connected. Waiting for audio...\n");
    while (lws_service(context, 100) >= 0) {
        // Main loop
    }

    lws_context_destroy(context);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
