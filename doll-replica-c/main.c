// filepath: /Users/satria/code/satriahrh/cocoa-fruit/doll-replica-c/main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <libwebsockets.h>

#define SERVER_ADDRESS  "localhost"
#define SERVER_PORT     8080
#define SERVER_PATH     "/ws"
#define IOBUF_SIZE      512

static volatile int interrupted = 0;

struct per_session_data {
    struct lws *wsi;
};

static int
callback_ws_client(struct lws *wsi,
                   enum lws_callback_reasons reason,
                   void *user, void *in, size_t len)
{
    switch (reason) {
    case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER: {
        const char *plainAuth = "John:Doe";
        unsigned char base64[128];
        int base64_len = EVP_EncodeBlock(base64, (const unsigned char*)plainAuth, strlen(plainAuth));

        // Prepare "Basic <base64>"
        char auth_header[160];
        snprintf(auth_header, sizeof(auth_header), "Basic %s", base64);

        unsigned char **p = (unsigned char **)in;
        unsigned char *end = (*p) + len;
        int n = lws_add_http_header_by_name(wsi,
            (const unsigned char *)"authorization",
            (const unsigned char *)auth_header,
            strlen(auth_header),
            p, end);
        if (n < 0) {
            lwsl_err("Failed to add Authorization header\n");
            return -1;
        }
        break;
    }
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        lwsl_user("Connected to server\n");
        ((struct per_session_data *)user)->wsi = wsi;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        printf("%.*s", (int)len, (char *)in);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        lwsl_err("Connection error\n");
        interrupted = 1;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        lwsl_user("Connection closed\n");
        interrupted = 1;
        break;

    default:
        break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "ws-protocol",
        callback_ws_client,
        sizeof(struct per_session_data),
        IOBUF_SIZE,
    },
    { NULL, NULL, 0, 0 } /* terminator */
};

static void
sigint_handler(int sig)
{
    interrupted = 1;
}

static void *
stdin_thread(void *arg)
{
    struct per_session_data *psd = arg;
    char buf[IOBUF_SIZE];

    while (!interrupted && fgets(buf, sizeof(buf), stdin)) {
        if (strcmp(buf, "exit\n") == 0) {
            interrupted = 1;
            break;
        }
        size_t n = strlen(buf);
        unsigned char *out = malloc(LWS_PRE + n);
        if (!out) {
            perror("malloc");
            break;
        }
        memcpy(out + LWS_PRE, buf, n);
        lws_write(psd->wsi, out + LWS_PRE, n, LWS_WRITE_TEXT);
        free(out);
    }
    return NULL;
}

int main(void)
{
    struct lws_context *context;
    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo = {0};
    struct per_session_data psd = {0};
    pthread_t tid;

    signal(SIGINT, sigint_handler);

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "lws init failed\n");
        return EXIT_FAILURE;
    }

    ccinfo.context  = context;
    ccinfo.address  = SERVER_ADDRESS;
    ccinfo.port     = SERVER_PORT;
    ccinfo.path     = SERVER_PATH;
    ccinfo.host     = lws_canonical_hostname(context);
    ccinfo.origin   = "origin";
    ccinfo.protocol = protocols[0].name;
    ccinfo.userdata = &psd;
    

    psd.wsi = lws_client_connect_via_info(&ccinfo);
    if (!psd.wsi) {
        fprintf(stderr, "Client connection failed\n");
        lws_context_destroy(context);
        return EXIT_FAILURE;
    }

    if (pthread_create(&tid, NULL, stdin_thread, &psd) != 0) {
        perror("pthread_create");
        interrupted = 1;
    }

    while (!interrupted)
        lws_service(context, 100);

    pthread_join(tid, NULL);
    lws_context_destroy(context);

    printf("Shutting down...\n");
    return EXIT_SUCCESS;
}