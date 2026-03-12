#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>
#include "device_tracker.h"
#include "orion_protocol.h"
#include "proxy_config.h"

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET http_socket_t;
#define HTTP_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
typedef int http_socket_t;
#define HTTP_INVALID_SOCKET (-1)
#endif

#define HTTP_MAX_REQUEST    8192
#define HTTP_MAX_RESPONSE   65536
#define HTTP_MAX_EVENTS     200

typedef struct {
    char  time_str[16];
    char  data[256];
} http_event_entry_t;

typedef struct {
    http_socket_t     listen_sock;
    uint16_t          port;
    device_tracker_t *tracker;
    orion_ctx_t      *orion_ctx;
    proxy_config_t   *config;
    volatile int     *stop_flag;

    /* Event ring buffer for live events */
    http_event_entry_t events[HTTP_MAX_EVENTS];
    int                event_write;
    int                event_count;

    /* Path to dashboard HTML file */
    char               html_path[512];

#ifdef _WIN32
    HANDLE             thread;
    CRITICAL_SECTION   event_lock;
#else
    pthread_t          thread;
#endif
} http_server_t;

/**
 * Initialize the HTTP server.
 */
int http_server_init(http_server_t *srv, uint16_t port,
                     device_tracker_t *tracker, orion_ctx_t *ctx,
                     proxy_config_t *config, volatile int *stop_flag);

/**
 * Start the HTTP server in a background thread.
 */
int http_server_start(http_server_t *srv);

/**
 * Push an event into the live event buffer (called from sniffer callback).
 */
void http_server_push_event(http_server_t *srv, const char *event_text);

/**
 * Stop the HTTP server.
 */
void http_server_stop(http_server_t *srv);

#endif /* HTTP_SERVER_H */
