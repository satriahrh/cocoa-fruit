#ifndef CONFIG_H
#define CONFIG_H

// WebSocket server configuration
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080
#define WEBSOCKET_PATH "/ws"

// Authentication configuration
#define AUTH_USERNAME "John"
#define AUTH_PASSWORD "Doe"

// Connection settings
#define PING_INTERVAL_SECONDS 30  // More IoT-friendly
#define MAX_MESSAGE_LENGTH 65536  // Back to reasonable size, large data handled dynamically
#define MAX_QUEUE_SIZE 10

// Logging levels
#define LOG_LEVELS (LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE)

#endif // CONFIG_H 