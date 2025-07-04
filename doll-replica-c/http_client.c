#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

// HTTP client state
bool http_initialized = false;
char current_jwt_token[MAX_JWT_TOKEN_LENGTH] = {0};

// Streaming session state
static int streaming_socket = -1;
static bool streaming_session_active = false;
static char streaming_session_id[64] = {0};

// Helper function to create HTTP request
static char* create_http_request(const char *method, const char *path, 
                                const char *headers, size_t body_length) {
    char *request = malloc(4096);
    if (!request) return NULL;
    
    snprintf(request, 4096,
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "%s"
        "Content-Length: %zu\r\n"
        "\r\n",
        method, path, HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT,
        headers ? headers : "",
        body_length
    );
    
    return request;
}

// Helper function to send HTTP request and receive response
static bool send_http_request(const char *request, const char *body, 
                             size_t body_length, char *response, size_t max_response) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    struct hostent *server = gethostbyname(HTTP_SERVER_ADDRESS);
    if (!server) {
        close(sock);
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(HTTP_SERVER_PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return false;
    }
    
    if (send(sock, request, strlen(request), 0) < 0) {
        close(sock);
        return false;
    }
    
    if (body && body_length > 0) {
        if (send(sock, body, body_length, 0) < 0) {
            close(sock);
            return false;
        }
    }
    
    ssize_t bytes_received = recv(sock, response, max_response - 1, 0);
    if (bytes_received < 0) {
        close(sock);
        return false;
    }
    
    response[bytes_received] = '\0';
    close(sock);
    return true;
}

bool http_init(void) {
    if (http_initialized) return true;
    
    printf("🔧 Initializing HTTP client...\n");
    http_initialized = true;
    return true;
}

void http_cleanup(void) {
    if (!http_initialized) return;
    
    printf("🧹 Cleaning up HTTP client...\n");
    http_initialized = false;
    memset(current_jwt_token, 0, sizeof(current_jwt_token));
    
    // Clean up streaming session if active
    if (streaming_session_active) {
        http_finish_streaming_session();
    }
}

bool http_health_check(void) {
    if (!http_initialized) return false;
    
    printf("🏥 Checking server health...\n");
    
    char *request = create_http_request("GET", "/api/v1/health", NULL, 0);
    if (!request) return false;
    
    char response[MAX_HTTP_RESPONSE_LENGTH];
    bool success = send_http_request(request, NULL, 0, response, sizeof(response));
    free(request);
    
    if (!success) return false;
    
    // Check if response contains "healthy"
    if (strstr(response, "healthy") != NULL) {
        printf("✅ Server is healthy\n");
        return true;
    }
    
    printf("❌ Server health check failed\n");
    return false;
}

bool http_get_jwt_token(char *jwt_token, size_t max_length) {
    if (!http_initialized) {
        printf("❌ HTTP client not initialized\n");
        return false;
    }
    
    printf("🔑 Getting JWT token...\n");
    
    char headers[512];
    snprintf(headers, sizeof(headers),
        "X-API-Key: %s\r\n"
        "X-API-Secret: %s\r\n"
        "Content-Type: application/json\r\n",
        HTTP_API_KEY, HTTP_API_SECRET
    );
    
    char *request = create_http_request("POST", "/api/v1/auth/token", headers, 0);
    if (!request) {
        printf("❌ Failed to create HTTP request\n");
        return false;
    }
    
    char response[MAX_HTTP_RESPONSE_LENGTH];
    bool success = send_http_request(request, NULL, 0, response, sizeof(response));
    free(request);
    
    if (!success) {
        printf("❌ Failed to send HTTP request\n");
        return false;
    }
    
    // Parse response for JWT token
    char *token_start = strstr(response, "\"token\":\"");
    if (token_start) {
        token_start += 9; // Skip "token":"
        char *token_end = strchr(token_start, '"');
        if (token_end) {
            size_t token_len = token_end - token_start;
            if (token_len < max_length) {
                strncpy(jwt_token, token_start, token_len);
                jwt_token[token_len] = '\0';
                printf("✅ JWT token obtained: %.20s...\n", jwt_token);
                return true;
            }
        }
    }
    
    printf("❌ Failed to parse JWT token from response\n");
    return false;
}

bool http_init_streaming_session(const char *jwt_token) {
    if (!http_initialized || !jwt_token) return false;
    
    printf("🚀 Initializing real-time streaming session...\n");
    
    // Create socket for streaming
    streaming_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (streaming_socket < 0) return false;
    
    struct hostent *server = gethostbyname(HTTP_SERVER_ADDRESS);
    if (!server) {
        close(streaming_socket);
        streaming_socket = -1;
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(HTTP_SERVER_PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(streaming_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(streaming_socket);
        streaming_socket = -1;
        return false;
    }
    
    // Send HTTP request headers
    char headers[512];
    snprintf(headers, sizeof(headers),
        "Authorization: Bearer %s\r\n"
        "Content-Type: audio/wav\r\n"
        "Transfer-Encoding: chunked\r\n",
        jwt_token
    );
    
    char request[1024];
    snprintf(request, sizeof(request),
        "POST /api/v1/audio/stream HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "%s"
        "\r\n",
        HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT, headers
    );
    
    if (send(streaming_socket, request, strlen(request), 0) < 0) {
        close(streaming_socket);
        streaming_socket = -1;
        return false;
    }
    
    streaming_session_active = true;
    printf("✅ Streaming session initialized\n");
    return true;
}

bool http_stream_audio_chunk(const unsigned char *chunk_data, size_t chunk_size) {
    if (!streaming_session_active || streaming_socket < 0 || !chunk_data || chunk_size == 0) {
        return false;
    }
    
    // Send chunk in chunked transfer encoding format
    char chunk_header[32];
    snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", chunk_size);
    
    if (send(streaming_socket, chunk_header, strlen(chunk_header), 0) < 0) {
        return false;
    }
    
    if (send(streaming_socket, chunk_data, chunk_size, 0) < 0) {
        return false;
    }
    
    if (send(streaming_socket, "\r\n", 2, 0) < 0) {
        return false;
    }
    
    return true;
}

bool http_finish_streaming_session(void) {
    if (!streaming_session_active || streaming_socket < 0) {
        return false;
    }
    
    printf("🏁 Finishing streaming session...\n");
    
    // Send end-of-stream marker
    if (send(streaming_socket, "0\r\n\r\n", 5, 0) < 0) {
        printf("❌ Failed to send end-of-stream marker\n");
    }
    
    // Read response
    char response[MAX_HTTP_RESPONSE_LENGTH];
    ssize_t bytes_received = recv(streaming_socket, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf("📥 Streaming session response: %s\n", response);
    }
    
    close(streaming_socket);
    streaming_socket = -1;
    streaming_session_active = false;
    
    printf("✅ Streaming session finished\n");
    return true;
}

bool http_stream_audio_realtime(const char *jwt_token, const unsigned char *audio_data, size_t data_size) {
    if (!http_initialized || !jwt_token || !audio_data || data_size == 0) return false;
    
    printf("📤 Streaming %zu bytes of audio data (real-time)...\n", data_size);
    
    char headers[512];
    snprintf(headers, sizeof(headers),
        "Authorization: Bearer %s\r\n"
        "Content-Type: audio/wav\r\n",
        jwt_token
    );
    
    char *request = create_http_request("POST", "/api/v1/audio/stream", headers, data_size);
    if (!request) return false;
    
    char response[MAX_HTTP_RESPONSE_LENGTH];
    bool success = send_http_request(request, (const char*)audio_data, data_size, response, sizeof(response));
    free(request);
    
    if (!success) return false;
    
    http_audio_response_t parsed_response;
    if (http_parse_audio_response(response, &parsed_response)) {
        printf("✅ Audio streamed successfully!\n");
        printf("   Session ID: %s\n", parsed_response.session_id);
        printf("   Message: %s\n", parsed_response.message);
        printf("   Note: Transcription will be sent via WebSocket\n");
        return true;
    }
    
    return false;
}

bool http_parse_audio_response(const char *response, http_audio_response_t *parsed_response) {
    if (!response || !parsed_response) return false;
    
    // Initialize response
    memset(parsed_response, 0, sizeof(http_audio_response_t));
    
    // Simple JSON parsing (for production, use a proper JSON parser)
    char *success_start = strstr(response, "\"success\":");
    if (success_start) {
        success_start += 10;
        if (strncmp(success_start, "true", 4) == 0) {
            parsed_response->success = true;
        }
    }
    
    // Parse message
    char *message_start = strstr(response, "\"message\":\"");
    if (message_start) {
        message_start += 11;
        char *message_end = strchr(message_start, '"');
        if (message_end) {
            size_t len = message_end - message_start;
            if (len < sizeof(parsed_response->message)) {
                strncpy(parsed_response->message, message_start, len);
                parsed_response->message[len] = '\0';
            }
        }
    }
    
    // Parse session_id
    char *session_start = strstr(response, "\"session_id\":\"");
    if (session_start) {
        session_start += 14;
        char *session_end = strchr(session_start, '"');
        if (session_end) {
            size_t len = session_end - session_start;
            if (len < sizeof(parsed_response->session_id)) {
                strncpy(parsed_response->session_id, session_start, len);
                parsed_response->session_id[len] = '\0';
            }
        }
    }
    
    // Parse text
    char *text_start = strstr(response, "\"text\":\"");
    if (text_start) {
        text_start += 8;
        char *text_end = strchr(text_start, '"');
        if (text_end) {
            size_t len = text_end - text_start;
            if (len < sizeof(parsed_response->text)) {
                strncpy(parsed_response->text, text_start, len);
                parsed_response->text[len] = '\0';
            }
        }
    }
    
    return true;
} 