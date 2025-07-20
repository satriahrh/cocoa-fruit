// websocket.c: WebSocket implementation using libwebsockets
#include "websocket.h"
#include <stdio.h>
#include <string.h>

static char ws_jwt_token[1024] = {0};
static void *ws_user_data = NULL;

static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                      void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (ws_user_data && len > 0) {
                // user_data is PaStream*
                extern int playback_write(void *, const void *, size_t);
                playback_write(ws_user_data, in, len);
            }
            break;
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
            char auth_header[1024];
            snprintf(auth_header, sizeof(auth_header), "Bearer %s", ws_jwt_token);
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

static struct lws_protocols ws_protocols[] = {
    { "audio-protocol", ws_callback, 0, 4096 },
    { NULL, NULL, 0, 0 }
};

struct lws_context *websocket_create_context(struct lws_protocols *protocols) {
    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols ? protocols : ws_protocols;
    return lws_create_context(&info);
}

struct lws *websocket_connect(struct lws_context *context, struct ws_config *cfg) {
    if (!cfg || !cfg->jwt_token) return NULL;
    strncpy(ws_jwt_token, cfg->jwt_token, sizeof(ws_jwt_token)-1);
    ws_user_data = cfg->user_data;
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = context;
    ccinfo.address = cfg->address;
    ccinfo.port = cfg->port;
    ccinfo.path = cfg->path;
    ccinfo.host = cfg->host;
    ccinfo.origin = cfg->origin;
    ccinfo.protocol = cfg->protocol;
    ccinfo.ssl_connection = 0;
    return lws_client_connect_via_info(&ccinfo);
}

void websocket_service_loop(struct lws_context *context) {
    while (lws_service(context, 100) >= 0) {}
}

void websocket_destroy(struct lws_context *context) {
    lws_context_destroy(context);
}
