#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <stdint.h>
#include <stddef.h>
#include "device_tracker.h"
#include "orion_protocol.h"
#include "proxy_config.h"
#include "orion_device_types.h"

#define PROXY_MAX_CLIENTS    16
#define PROXY_DEFAULT_PORT   9100
#define PROXY_BUF_SIZE       4096
#define PROXY_LINE_MAX       1024

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET proxy_socket_t;
#define PROXY_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int proxy_socket_t;
#define PROXY_INVALID_SOCKET (-1)
#endif

/**
 * Client connection state.
 */
typedef struct {
    proxy_socket_t sock;
    char           addr_str[64];       /* "IP:port" for logging */
    int            subscribed_events;  /* 1 = receives event broadcasts */
    int            subscribed_raw;     /* 1 = receives raw hex packets */
    char           recv_buf[PROXY_BUF_SIZE];
    size_t         recv_len;
} proxy_client_t;

/**
 * Proxy server state.
 */
typedef struct {
    proxy_socket_t   listen_sock;
    uint16_t         port;
    proxy_client_t   clients[PROXY_MAX_CLIENTS];
    int              client_count;
    device_tracker_t *tracker;         /* Shared device tracker */
    orion_ctx_t      *orion_ctx;       /* For sending commands (MASTER mode) */
    proxy_config_t   *config;           /* Shared configuration (runtime changes) */
    volatile int     *stop_flag;
    int              verbose;

#ifdef _WIN32
    CRITICAL_SECTION clients_lock;
#endif
} proxy_server_t;

/**
 * Initialize the proxy TCP server.
 *
 * @param srv       Server state to initialize.
 * @param port      TCP port to listen on (0 = default 9100).
 * @param tracker   Shared device tracker.
 * @param orion_ctx Orion protocol context (for sending commands).
 * @param stop_flag Pointer to stop flag.
 * @return          0 on success, -1 on error.
 */
int proxy_server_init(proxy_server_t *srv, uint16_t port,
                      device_tracker_t *tracker, orion_ctx_t *orion_ctx,
                      volatile int *stop_flag);

/**
 * Start listening for client connections.
 * @return 0 on success, -1 on error.
 */
int proxy_server_start(proxy_server_t *srv);

/**
 * Poll for new connections and process client commands.
 * Non-blocking — call this from the main loop.
 * @return Number of clients processed, -1 on error.
 */
int proxy_server_poll(proxy_server_t *srv);

/**
 * Broadcast a message to all connected clients that are subscribed to events.
 */
void proxy_server_broadcast_event(proxy_server_t *srv, const char *message);

/**
 * Broadcast a raw packet hex dump to subscribed clients.
 */
void proxy_server_broadcast_raw(proxy_server_t *srv, const uint8_t *raw,
                                size_t raw_len, const orion_packet_t *pkt);

/**
 * Send a response to a specific client.
 */
void proxy_server_send(proxy_server_t *srv, int client_idx, const char *message);

/**
 * Shut down the server and disconnect all clients.
 */
void proxy_server_shutdown(proxy_server_t *srv);

#endif /* PROXY_SERVER_H */
