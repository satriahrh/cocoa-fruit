// websocket.h: WebSocket interface using libwebsockets
#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <libwebsockets.h>

struct ws_config {
    const char *address;
    int port;
    const char *path;
    const char *host;
    const char *origin;
    const char *protocol;
    const char *jwt_token;
    void *user_data; // e.g. PaStream*
};

struct lws_context *websocket_create_context(struct lws_protocols *protocols);
struct lws *websocket_connect(struct lws_context *context, struct ws_config *cfg);
void websocket_service_loop(struct lws_context *context);
void websocket_destroy(struct lws_context *context);

#endif // WEBSOCKET_H
