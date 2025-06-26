#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <libwebsockets.h>
#include <signal.h>

// WebSocket client functions
int init_websocket_client(void);
void cleanup_websocket_client(void);
int connect_to_server(void);
void disconnect_from_server(void);

// Global WebSocket variables (extern for access from other modules)
extern struct lws *websocket_connection;
extern struct lws_context *websocket_context;
extern int should_exit;

// Signal handler
void signal_handler(int signal);

// WebSocket callback function
int websocket_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len);

#endif // WEBSOCKET_CLIENT_H 