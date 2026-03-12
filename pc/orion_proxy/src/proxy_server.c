#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "proxy_server.h"
#include "orion_commands.h"
#include "orion_device_types.h"
#include "proxy_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef _WIN32
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#endif

/* ─── Platform helpers ─────────────────────────────────────────────────── */

static void set_nonblocking(proxy_socket_t sock)
{
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static void close_socket(proxy_socket_t sock)
{
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

static int socket_error(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int would_block(void)
{
#ifdef _WIN32
    return (WSAGetLastError() == WSAEWOULDBLOCK);
#else
    return (errno == EAGAIN || errno == EWOULDBLOCK);
#endif
}

/* ─── Client lock helpers ──────────────────────────────────────────────── */

static void srv_lock(proxy_server_t *srv)
{
#ifdef _WIN32
    EnterCriticalSection(&srv->clients_lock);
#endif
}

static void srv_unlock(proxy_server_t *srv)
{
#ifdef _WIN32
    LeaveCriticalSection(&srv->clients_lock);
#endif
}

/* ─── Command processing ──────────────────────────────────────────────── */

static void process_client_command(proxy_server_t *srv, int idx, char *line)
{
    char buf[PROXY_BUF_SIZE];
    int n;
    unsigned int addr, zone;

    /* Trim trailing whitespace */
    {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) {
            line[--len] = '\0';
        }
    }

    if (srv->verbose) {
        printf("[PROXY] Client %s: %s\n", srv->clients[idx].addr_str, line);
        fflush(stdout);
    }

    /* PING */
    if (strcmp(line, "PING") == 0) {
        proxy_server_send(srv, idx, "PONG\n");
        return;
    }

    /* GET_STATUS */
    if (strcmp(line, "GET_STATUS") == 0) {
        n = tracker_format_status(srv->tracker, buf, sizeof(buf));
        if (n > 0) proxy_server_send(srv, idx, buf);
        return;
    }

    /* GET_DEVICES */
    if (strcmp(line, "GET_DEVICES") == 0) {
        n = tracker_format_devices(srv->tracker, buf, sizeof(buf));
        if (n > 0) {
            proxy_server_send(srv, idx, buf);
        } else {
            proxy_server_send(srv, idx, "DEVICES,count=0\n");
        }
        proxy_server_send(srv, idx, "END\n");
        return;
    }

    /* GET_DEVICE,<addr> */
    if (sscanf(line, "GET_DEVICE,%u", &addr) == 1) {
        n = tracker_format_device(srv->tracker, (uint8_t)addr, buf, sizeof(buf));
        if (n > 0) proxy_server_send(srv, idx, buf);
        return;
    }

    /* GET_EVENTS or GET_EVENTS,<count> */
    if (strncmp(line, "GET_EVENTS", 10) == 0) {
        int max_events = 50;
        if (sscanf(line, "GET_EVENTS,%d", &max_events) != 1) {
            max_events = 50;
        }
        n = tracker_format_events(srv->tracker, buf, sizeof(buf), max_events);
        if (n > 0) proxy_server_send(srv, idx, buf);
        proxy_server_send(srv, idx, "END\n");
        return;
    }

    /* SUBSCRIBE_EVENTS — receive event broadcasts */
    if (strcmp(line, "SUBSCRIBE_EVENTS") == 0) {
        srv->clients[idx].subscribed_events = 1;
        proxy_server_send(srv, idx, "OK,subscribed to events\n");
        return;
    }

    /* UNSUBSCRIBE_EVENTS */
    if (strcmp(line, "UNSUBSCRIBE_EVENTS") == 0) {
        srv->clients[idx].subscribed_events = 0;
        proxy_server_send(srv, idx, "OK,unsubscribed from events\n");
        return;
    }

    /* SUBSCRIBE_RAW — receive raw packet hex dumps */
    if (strcmp(line, "SUBSCRIBE_RAW") == 0) {
        srv->clients[idx].subscribed_raw = 1;
        proxy_server_send(srv, idx, "OK,subscribed to raw packets\n");
        return;
    }

    /* UNSUBSCRIBE_RAW */
    if (strcmp(line, "UNSUBSCRIBE_RAW") == 0) {
        srv->clients[idx].subscribed_raw = 0;
        proxy_server_send(srv, idx, "OK,unsubscribed from raw packets\n");
        return;
    }

    /* SCAN — request active scan (MASTER mode only) */
    if (strcmp(line, "SCAN") == 0) {
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (srv->config && !srv->config->master_mode) {
            proxy_server_send(srv, idx, "ERROR,scan requires MASTER mode (SET,master_mode=1)\n");
            return;
        }
        proxy_server_send(srv, idx, "OK,scan queued (results broadcast as DEVICE_FOUND events)\n");
        return;
    }

    /* SCAN_DEVICE,<addr> — query single device info (MASTER mode) */
    if (sscanf(line, "SCAN_DEVICE,%u", &addr) == 1) {
        orion_device_info_t info;
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (orion_cmd_read_device_info(srv->orion_ctx, (uint8_t)addr, &info) == 0) {
            srv->tracker->devices[addr].device_type = info.device_type;
            srv->tracker->devices[addr].fw_major = info.firmware_major;
            srv->tracker->devices[addr].fw_minor = info.firmware_minor;
            srv->tracker->devices[addr].online = 1;
            snprintf(buf, sizeof(buf),
                "DEVICE_INFO,%u,model=%s,type=0x%02X,category=%s,"
                "fw=%d.%d,zones=%d,name=%s\n",
                addr,
                orion_device_model(info.device_type),
                info.device_type,
                orion_device_category_name(orion_device_category(info.device_type)),
                info.firmware_major, info.firmware_minor,
                orion_device_zone_count(info.device_type),
                orion_device_type_name(info.device_type));
        } else {
            snprintf(buf, sizeof(buf), "ERROR,no response from device %u\n", addr);
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* READ_STATUS,<addr> — read device status (MASTER mode) */
    if (sscanf(line, "READ_STATUS,%u", &addr) == 1) {
        orion_device_status_t status;
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (orion_cmd_read_status(srv->orion_ctx, (uint8_t)addr, &status) == 0) {
            snprintf(buf, sizeof(buf),
                "DEVICE_STATUS,%u,status1=0x%02X(%s),status2=0x%02X(%s)\n",
                addr,
                status.status1, orion_status_str(status.status1),
                status.status2, orion_status_str(status.status2));
        } else {
            snprintf(buf, sizeof(buf), "ERROR,cannot read status from device %u\n", addr);
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* GET_CONFIG — show current configuration */
    if (strcmp(line, "GET_CONFIG") == 0) {
        if (srv->config) {
            n = config_format(srv->config, buf, sizeof(buf));
            if (n > 0) proxy_server_send(srv, idx, buf);
        } else {
            proxy_server_send(srv, idx, "ERROR,config not available\n");
        }
        return;
    }

    /* SET,<key>=<value> — change runtime configuration */
    if (strncmp(line, "SET,", 4) == 0) {
        char *eq_pos = strchr(line + 4, '=');
        if (eq_pos && srv->config) {
            char set_key[128], set_val[256];
            size_t key_len = (size_t)(eq_pos - (line + 4));
            if (key_len < sizeof(set_key)) {
                memcpy(set_key, line + 4, key_len);
                set_key[key_len] = '\0';
                strncpy(set_val, eq_pos + 1, sizeof(set_val) - 1);
                set_val[sizeof(set_val) - 1] = '\0';
                if (config_set(srv->config, set_key, set_val) == 0) {
                    snprintf(buf, sizeof(buf), "OK,%s=%s\n", set_key, set_val);
                } else {
                    snprintf(buf, sizeof(buf), "ERROR,unknown config key: %s\n", set_key);
                }
            } else {
                snprintf(buf, sizeof(buf), "ERROR,key too long\n");
            }
        } else {
            snprintf(buf, sizeof(buf), "ERROR,format: SET,key=value\n");
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* SAVE_CONFIG — persist configuration to file */
    if (strcmp(line, "SAVE_CONFIG") == 0) {
        if (srv->config) {
            if (config_save_file(srv->config, CONFIG_FILE_NAME) == 0) {
                proxy_server_send(srv, idx, "OK,config saved\n");
            } else {
                proxy_server_send(srv, idx, "ERROR,failed to save config\n");
            }
        } else {
            proxy_server_send(srv, idx, "ERROR,config not available\n");
        }
        return;
    }

    /* DEBUG,<level> — change debug level at runtime (0-3) */
    if (strncmp(line, "DEBUG,", 6) == 0) {
        int level = atoi(line + 6);
        if (level >= 0 && level <= 3 && srv->config) {
            srv->config->debug_level = level;
            snprintf(buf, sizeof(buf), "OK,debug_level=%d (%s)\n", level,
                     level == 0 ? "OFF" :
                     level == 1 ? "EVENTS" :
                     level == 2 ? "PACKETS" : "HEX_DUMP");
        } else {
            snprintf(buf, sizeof(buf), "ERROR,debug level must be 0-3\n");
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* DEBUG — show current debug level */
    if (strcmp(line, "DEBUG") == 0) {
        if (srv->config) {
            snprintf(buf, sizeof(buf),
                "DEBUG_STATUS:\n"
                "  debug_level = %d (%s)\n"
                "  verbose = %d\n"
                "  log_to_file = %d (%s)\n"
                "  show_timestamps = %d\n"
                "END\n",
                srv->config->debug_level,
                srv->config->debug_level == 0 ? "OFF" :
                srv->config->debug_level == 1 ? "EVENTS" :
                srv->config->debug_level == 2 ? "PACKETS" : "HEX_DUMP",
                srv->config->verbose,
                srv->config->log_to_file, srv->config->log_file,
                srv->config->show_timestamps);
        } else {
            snprintf(buf, sizeof(buf), "ERROR,config not available\n");
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* DEVICE_TYPES — list all known Bolid device types */
    if (strcmp(line, "DEVICE_TYPES") == 0) {
        static const uint8_t known_types[] = {
            0x01, 0x02, 0x04, 0x05, 0x06, 0x0A, 0x0B, 0x0E, 0x0F, 0x10,
            0x11, 0x13, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
            0x1E, 0x22, 0x24, 0x29, 0x2A, 0x2C, 0x2D, 0x34, 0x37
        };
        size_t ti;
        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "DEVICE_TYPES:\n");
        for (ti = 0; ti < sizeof(known_types) / sizeof(known_types[0]) && pos < (int)sizeof(buf) - 128; ++ti) {
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "  0x%02X = %-16s %-12s zones=%d\n",
                known_types[ti],
                orion_device_model(known_types[ti]),
                orion_device_category_name(orion_device_category(known_types[ti])),
                orion_device_zone_count(known_types[ti]));
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "END\n");
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* ARM,<addr>,<zone> */
    if (sscanf(line, "ARM,%u,%u", &addr, &zone) == 2) {
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (orion_cmd_arm(srv->orion_ctx, (uint8_t)addr, (uint8_t)zone) == 0) {
            snprintf(buf, sizeof(buf), "OK,ARM sent to device %u zone %u\n", addr, zone);
        } else {
            snprintf(buf, sizeof(buf), "ERROR,ARM failed for device %u zone %u\n", addr, zone);
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* DISARM,<addr>,<zone> */
    if (sscanf(line, "DISARM,%u,%u", &addr, &zone) == 2) {
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (orion_cmd_disarm(srv->orion_ctx, (uint8_t)addr, (uint8_t)zone) == 0) {
            snprintf(buf, sizeof(buf), "OK,DISARM sent to device %u zone %u\n", addr, zone);
        } else {
            snprintf(buf, sizeof(buf), "ERROR,DISARM failed for device %u zone %u\n", addr, zone);
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* RESET_ALARM,<addr>,<zone> */
    if (sscanf(line, "RESET_ALARM,%u,%u", &addr, &zone) == 2) {
        if (!srv->orion_ctx || srv->orion_ctx->port == ORION_INVALID_PORT) {
            proxy_server_send(srv, idx, "ERROR,no RS-485 connection\n");
            return;
        }
        if (orion_cmd_reset_alarm(srv->orion_ctx, (uint8_t)addr, (uint8_t)zone) == 0) {
            snprintf(buf, sizeof(buf), "OK,RESET_ALARM sent to device %u zone %u\n", addr, zone);
        } else {
            snprintf(buf, sizeof(buf), "ERROR,RESET_ALARM failed for device %u zone %u\n", addr, zone);
        }
        proxy_server_send(srv, idx, buf);
        return;
    }

    /* HELP */
    if (strcmp(line, "HELP") == 0) {
        proxy_server_send(srv, idx,
            "COMMANDS:\n"
            "\n  --- Query ---\n"
            "  PING                      Test connection\n"
            "  GET_STATUS                System status (packets, errors, uptime)\n"
            "  GET_DEVICES               List all known devices on bus\n"
            "  GET_DEVICE,<addr>         Single device details\n"
            "  GET_EVENTS[,<count>]      Recent events (default 50)\n"
            "  DEVICE_TYPES              List all known Bolid device types\n"
            "\n  --- Subscription ---\n"
            "  SUBSCRIBE_EVENTS          Receive real-time events\n"
            "  UNSUBSCRIBE_EVENTS        Stop receiving events\n"
            "  SUBSCRIBE_RAW             Receive raw packet hex dumps\n"
            "  UNSUBSCRIBE_RAW           Stop receiving raw packets\n"
            "\n  --- MASTER Mode (active bus commands) ---\n"
            "  SCAN                      Scan all 127 addresses\n"
            "  SCAN_DEVICE,<addr>        Query single device info\n"
            "  READ_STATUS,<addr>        Read device status\n"
            "  ARM,<addr>,<zone>         Arm zone\n"
            "  DISARM,<addr>,<zone>      Disarm zone\n"
            "  RESET_ALARM,<addr>,<zone> Reset alarm\n"
            "\n  --- Configuration ---\n"
            "  GET_CONFIG                Show all settings\n"
            "  SET,<key>=<value>         Change a setting\n"
            "  SAVE_CONFIG               Save settings to file\n"
            "\n  --- Debug ---\n"
            "  DEBUG                     Show debug status\n"
            "  DEBUG,<0-3>               Set debug level (0=off,1=events,2=packets,3=hex)\n"
            "\n  --- Session ---\n"
            "  HELP                      This help text\n"
            "  QUIT                      Disconnect\n"
            "END\n");
        return;
    }

    /* QUIT */
    if (strcmp(line, "QUIT") == 0) {
        proxy_server_send(srv, idx, "BYE\n");
        /* Mark for disconnect */
        close_socket(srv->clients[idx].sock);
        srv->clients[idx].sock = PROXY_INVALID_SOCKET;
        return;
    }

    /* Unknown command */
    snprintf(buf, sizeof(buf), "ERROR,unknown command: %s\n", line);
    proxy_server_send(srv, idx, buf);
}

/* ─── Server init / start ──────────────────────────────────────────────── */

int proxy_server_init(proxy_server_t *srv, uint16_t port,
                      device_tracker_t *tracker, orion_ctx_t *orion_ctx,
                      volatile int *stop_flag)
{
    int i;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[PROXY] WSAStartup failed: %d\n", WSAGetLastError());
        return -1;
    }
    InitializeCriticalSection(&srv->clients_lock);
#endif

    srv->listen_sock = PROXY_INVALID_SOCKET;
    srv->port = port ? port : PROXY_DEFAULT_PORT;
    srv->client_count = 0;
    srv->tracker = tracker;
    srv->orion_ctx = orion_ctx;
    srv->stop_flag = stop_flag;
    srv->verbose = 1;

    for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
        srv->clients[i].sock = PROXY_INVALID_SOCKET;
        srv->clients[i].subscribed_events = 0;
        srv->clients[i].subscribed_raw = 0;
        srv->clients[i].recv_len = 0;
    }

    return 0;
}

int proxy_server_start(proxy_server_t *srv)
{
    struct sockaddr_in addr;
    int opt;

    srv->listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_sock == PROXY_INVALID_SOCKET) {
        printf("[PROXY] Failed to create socket: %d\n", socket_error());
        return -1;
    }

    /* Allow address reuse */
    opt = 1;
    setsockopt(srv->listen_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(srv->port);

    if (bind(srv->listen_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[PROXY] Bind failed on port %d: %d\n", srv->port, socket_error());
        close_socket(srv->listen_sock);
        srv->listen_sock = PROXY_INVALID_SOCKET;
        return -1;
    }

    if (listen(srv->listen_sock, 5) != 0) {
        printf("[PROXY] Listen failed: %d\n", socket_error());
        close_socket(srv->listen_sock);
        srv->listen_sock = PROXY_INVALID_SOCKET;
        return -1;
    }

    set_nonblocking(srv->listen_sock);

    printf("[PROXY] TCP server listening on port %d\n", srv->port);
    printf("[PROXY] Clients can connect via: telnet localhost %d\n", srv->port);
    fflush(stdout);

    return 0;
}

/* ─── Poll (non-blocking) ─────────────────────────────────────────────── */

int proxy_server_poll(proxy_server_t *srv)
{
    struct sockaddr_in client_addr;
    int addr_len;
    proxy_socket_t new_sock;
    int i, n, processed;
    char *line_end;

    if (srv->listen_sock == PROXY_INVALID_SOCKET) {
        return -1;
    }

    processed = 0;

    /* Accept new connections */
    addr_len = sizeof(client_addr);
    new_sock = accept(srv->listen_sock, (struct sockaddr *)&client_addr,
                      (void *)&addr_len);

    if (new_sock != PROXY_INVALID_SOCKET) {
        /* Find free slot */
        int slot = -1;
        srv_lock(srv);
        for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
            if (srv->clients[i].sock == PROXY_INVALID_SOCKET) {
                slot = i;
                break;
            }
        }

        if (slot >= 0) {
            set_nonblocking(new_sock);
            srv->clients[slot].sock = new_sock;
            srv->clients[slot].subscribed_events = 1; /* Auto-subscribe to events */
            srv->clients[slot].subscribed_raw = 0;
            srv->clients[slot].recv_len = 0;
            snprintf(srv->clients[slot].addr_str, sizeof(srv->clients[slot].addr_str),
                     "%s:%d", inet_ntoa(client_addr.sin_addr),
                     ntohs(client_addr.sin_port));
            srv->client_count++;

            printf("[PROXY] Client connected: %s (slot %d, total %d)\n",
                   srv->clients[slot].addr_str, slot, srv->client_count);
            fflush(stdout);

            /* Send welcome */
            proxy_server_send(srv, slot,
                "ORION_PROXY v1.0\n"
                "Type HELP for commands, SUBSCRIBE_EVENTS for real-time feed.\n"
                "READY\n");
        } else {
            printf("[PROXY] Max clients reached, rejecting connection\n");
            fflush(stdout);
            close_socket(new_sock);
        }
        srv_unlock(srv);
    }

    /* Process existing clients */
    for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
        if (srv->clients[i].sock == PROXY_INVALID_SOCKET) {
            continue;
        }

        /* Try to read data */
        n = recv(srv->clients[i].sock,
                 srv->clients[i].recv_buf + srv->clients[i].recv_len,
                 (int)(PROXY_BUF_SIZE - srv->clients[i].recv_len - 1),
                 0);

        if (n > 0) {
            srv->clients[i].recv_len += n;
            srv->clients[i].recv_buf[srv->clients[i].recv_len] = '\0';

            /* Process complete lines */
            while ((line_end = strchr(srv->clients[i].recv_buf, '\n')) != NULL) {
                *line_end = '\0';
                process_client_command(srv, i, srv->clients[i].recv_buf);
                processed++;

                /* Shift remaining data */
                {
                    size_t consumed = (size_t)(line_end - srv->clients[i].recv_buf) + 1;
                    size_t remaining = srv->clients[i].recv_len - consumed;
                    if (remaining > 0) {
                        memmove(srv->clients[i].recv_buf,
                                line_end + 1, remaining);
                    }
                    srv->clients[i].recv_len = remaining;
                    srv->clients[i].recv_buf[remaining] = '\0';
                }

                /* Check if client was disconnected during command processing */
                if (srv->clients[i].sock == PROXY_INVALID_SOCKET) {
                    break;
                }
            }

            /* Buffer overflow protection */
            if (srv->clients[i].recv_len >= PROXY_BUF_SIZE - 1) {
                srv->clients[i].recv_len = 0;
            }

        } else if (n == 0) {
            /* Client disconnected */
            printf("[PROXY] Client disconnected: %s\n",
                   srv->clients[i].addr_str);
            fflush(stdout);
            close_socket(srv->clients[i].sock);
            srv->clients[i].sock = PROXY_INVALID_SOCKET;
            srv->clients[i].recv_len = 0;
            srv->client_count--;

        } else if (!would_block()) {
            /* Real error */
            printf("[PROXY] Client error: %s (err=%d)\n",
                   srv->clients[i].addr_str, socket_error());
            fflush(stdout);
            close_socket(srv->clients[i].sock);
            srv->clients[i].sock = PROXY_INVALID_SOCKET;
            srv->clients[i].recv_len = 0;
            srv->client_count--;
        }
        /* else: would_block = no data available, continue */
    }

    return processed;
}

/* ─── Send / Broadcast ─────────────────────────────────────────────────── */

void proxy_server_send(proxy_server_t *srv, int client_idx, const char *message)
{
    size_t len;

    if (client_idx < 0 || client_idx >= PROXY_MAX_CLIENTS) return;
    if (srv->clients[client_idx].sock == PROXY_INVALID_SOCKET) return;

    len = strlen(message);
    send(srv->clients[client_idx].sock, message, (int)len, 0);
}

void proxy_server_broadcast_event(proxy_server_t *srv, const char *message)
{
    int i;
    size_t len;

    len = strlen(message);

    srv_lock(srv);
    for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
        if (srv->clients[i].sock != PROXY_INVALID_SOCKET &&
            srv->clients[i].subscribed_events) {
            send(srv->clients[i].sock, message, (int)len, 0);
        }
    }
    srv_unlock(srv);
}

void proxy_server_broadcast_raw(proxy_server_t *srv, const uint8_t *raw,
                                size_t raw_len, const orion_packet_t *pkt)
{
    char buf[PROXY_BUF_SIZE];
    int n;
    size_t i;
    int pos;
    uint8_t addr;

    addr = pkt->address;
    if (pkt->is_encrypted && addr >= 0x80) {
        addr = addr - 0x80;
    }

    pos = snprintf(buf, sizeof(buf), "RAW,addr=%d,len=%d,enc=%d,hex=",
                   addr, pkt->length, pkt->is_encrypted);

    for (i = 0; i < raw_len && pos < (int)sizeof(buf) - 4; ++i) {
        n = snprintf(buf + pos, sizeof(buf) - pos, "%02X", raw[i]);
        pos += n;
    }
    buf[pos++] = '\n';
    buf[pos] = '\0';

    srv_lock(srv);
    for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
        if (srv->clients[i].sock != PROXY_INVALID_SOCKET &&
            srv->clients[i].subscribed_raw) {
            send(srv->clients[i].sock, buf, pos, 0);
        }
    }
    srv_unlock(srv);
}

/* ─── Shutdown ─────────────────────────────────────────────────────────── */

void proxy_server_shutdown(proxy_server_t *srv)
{
    int i;

    printf("[PROXY] Shutting down server...\n");
    fflush(stdout);

    for (i = 0; i < PROXY_MAX_CLIENTS; ++i) {
        if (srv->clients[i].sock != PROXY_INVALID_SOCKET) {
            send(srv->clients[i].sock, "SERVER_SHUTDOWN\n", 16, 0);
            close_socket(srv->clients[i].sock);
            srv->clients[i].sock = PROXY_INVALID_SOCKET;
        }
    }

    if (srv->listen_sock != PROXY_INVALID_SOCKET) {
        close_socket(srv->listen_sock);
        srv->listen_sock = PROXY_INVALID_SOCKET;
    }

#ifdef _WIN32
    DeleteCriticalSection(&srv->clients_lock);
    WSACleanup();
#endif

    printf("[PROXY] Server stopped.\n");
    fflush(stdout);
}
