// Minimal doll-replica: Connects to a WebSocket, receives PCM audio, and plays it using PortAudio.
// This example uses libwebsockets for WebSocket and PortAudio for playback.
// You must link with -lwebsockets -lportaudio when compiling.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "playback.h"
#include "websocket.h"




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



int main(int argc, char **argv) {
    char jwt_token[MAX_JWT_TOKEN_LENGTH] = {0};
    if (!get_jwt_token(jwt_token, sizeof(jwt_token))) {
        printf("Failed to get JWT token\n");
        return 1;
    }
    printf("JWT token: %.20s...\n", jwt_token);

    // Initialize playback
    void *stream = NULL;
    if (!playback_init((void**)&stream)) {
        printf("Failed to initialize playback\n");
        return 1;
    }

    // Setup websocket config
    struct ws_config cfg = {
        .address = SERVER_ADDRESS,
        .port = SERVER_PORT,
        .path = WEBSOCKET_PATH,
        .host = SERVER_ADDRESS,
        .origin = SERVER_ADDRESS,
        .protocol = "audio-protocol",
        .jwt_token = jwt_token,
        .user_data = stream
    };

    struct lws_context *context = websocket_create_context(NULL);
    if (!context) {
        printf("Failed to create websocket context\n");
        playback_close(stream);
        return 1;
    }
    struct lws *wsi = websocket_connect(context, &cfg);
    if (!wsi) {
        printf("Failed to connect to WebSocket (%s:%d%s)\n", SERVER_ADDRESS, SERVER_PORT, WEBSOCKET_PATH);
        websocket_destroy(context);
        playback_close(stream);
        return 1;
    }

    printf("Connected. Waiting for audio...\n");
    websocket_service_loop(context);

    websocket_destroy(context);
    playback_close(stream);
    return 0;
}
