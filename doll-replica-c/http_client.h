#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

// HTTP client configuration
#define HTTP_SERVER_ADDRESS "127.0.0.1"
#define HTTP_SERVER_PORT 8080
#define HTTP_API_KEY "John"
#define HTTP_API_SECRET "Doe"
#define MAX_JWT_TOKEN_LENGTH 1024
#define MAX_HTTP_RESPONSE_LENGTH 4096

// HTTP client functions
bool http_init(void);
void http_cleanup(void);

// Authentication
bool http_get_jwt_token(char *jwt_token, size_t max_length);

// Health check
bool http_health_check(void);

// Real-time audio streaming (sends transcription via WebSocket)
bool http_stream_audio_realtime(const char *jwt_token, const unsigned char *audio_data, size_t data_size);

// Real-time chunk streaming (for streaming individual chunks during recording)
bool http_init_streaming_session(const char *jwt_token);
bool http_stream_audio_chunk(const unsigned char *chunk_data, size_t chunk_size);
bool http_finish_streaming_session(void);

// Response parsing
typedef struct {
    bool success;
    char message[256];
    char session_id[64];
    char text[1024];
} http_audio_response_t;

bool http_parse_audio_response(const char *response, http_audio_response_t *parsed_response);

// HTTP client state
extern bool http_initialized;
extern char current_jwt_token[MAX_JWT_TOKEN_LENGTH];

#endif // HTTP_CLIENT_H 