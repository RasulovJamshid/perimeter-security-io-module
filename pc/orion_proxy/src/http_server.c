#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include "http_server.h"
#include "proxy_config.h"
#include "orion_device_types.h"
#include "orion_commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#endif

/* ─── Platform helpers ─────────────────────────────────────────────────── */

static void http_close_socket(http_socket_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

/* ─── Simple HTTP request parser ───────────────────────────────────────── */

typedef struct {
    char method[8];          /* GET or POST */
    char path[256];
    char body[4096];
    int  body_len;
    int  content_length;
} http_request_t;

static int parse_http_request(const char *raw, int raw_len, http_request_t *req)
{
    const char *line_end, *body_start;
    const char *cl;

    memset(req, 0, sizeof(*req));

    if (raw_len < 10) return -1;

    /* Parse request line: METHOD /path HTTP/1.x */
    if (sscanf(raw, "%7s %255s", req->method, req->path) != 2) {
        return -1;
    }

    /* Find Content-Length */
    cl = strstr(raw, "Content-Length:");
    if (!cl) cl = strstr(raw, "content-length:");
    if (cl) {
        req->content_length = atoi(cl + 15);
    }

    /* Find body (after \r\n\r\n) */
    body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req->body_len = raw_len - (int)(body_start - raw);
        if (req->body_len > (int)sizeof(req->body) - 1) {
            req->body_len = (int)sizeof(req->body) - 1;
        }
        if (req->body_len > 0) {
            memcpy(req->body, body_start, req->body_len);
            req->body[req->body_len] = '\0';
        }
    }

    return 0;
}

/* ─── Response helpers ─────────────────────────────────────────────────── */

static void send_response(http_socket_t sock, int code, const char *content_type,
                          const char *body, int body_len)
{
    char header[512];
    int header_len;
    const char *status_text = (code == 200) ? "OK" :
                              (code == 404) ? "Not Found" : "Bad Request";

    header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        code, status_text, content_type, body_len);

    send(sock, header, header_len, 0);
    if (body && body_len > 0) {
        send(sock, body, body_len, 0);
    }
}

static void send_json(http_socket_t sock, const char *json)
{
    send_response(sock, 200, "application/json", json, (int)strlen(json));
}

static void send_text(http_socket_t sock, const char *text)
{
    send_response(sock, 200, "text/plain", text, (int)strlen(text));
}

/* ─── JSON escaping helper ─────────────────────────────────────────────── */

static int json_escape(const char *src, char *dst, int dst_size)
{
    int i = 0, o = 0;
    while (src[i] && o < dst_size - 2) {
        if (src[i] == '"') { dst[o++] = '\\'; dst[o++] = '"'; }
        else if (src[i] == '\\') { dst[o++] = '\\'; dst[o++] = '\\'; }
        else if (src[i] == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
        else if (src[i] == '\r') { /* skip */ }
        else if (src[i] == '\t') { dst[o++] = '\\'; dst[o++] = 't'; }
        else { dst[o++] = src[i]; }
        i++;
    }
    dst[o] = '\0';
    return o;
}

/* ─── Simple JSON value extractor ──────────────────────────────────────── */

static int json_get_string(const char *json, const char *key, char *out, int out_size)
{
    char pattern[128];
    const char *p, *start, *end;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) return -1;

    p = strchr(p + strlen(pattern), ':');
    if (!p) return -1;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '"') {
        start = p + 1;
        end = strchr(start, '"');
        if (!end) return -1;
        if (end - start >= out_size) return -1;
        memcpy(out, start, end - start);
        out[end - start] = '\0';
    } else {
        /* Number or bare value */
        start = p;
        end = start;
        while (*end && *end != ',' && *end != '}' && *end != ' ') end++;
        if (end - start >= out_size) return -1;
        memcpy(out, start, end - start);
        out[end - start] = '\0';
    }
    return 0;
}

/* ─── Dashboard HTML serving ───────────────────────────────────────────── */

static void serve_dashboard(http_server_t *srv, http_socket_t sock)
{
    FILE *f;
    long file_size;
    char *buf;

    f = fopen(srv->html_path, "rb");
    if (!f) {
        /* Try alternate paths */
        f = fopen("dashboard.html", "rb");
    }
    if (!f) {
        f = fopen("gui/index.html", "rb");
    }
    if (!f) {
        const char *fallback =
            "<!DOCTYPE html><html><head><title>Orion Proxy</title></head>"
            "<body style='background:#0f1117;color:#e4e6f0;font-family:sans-serif;"
            "display:flex;align-items:center;justify-content:center;height:100vh'>"
            "<div style='text-align:center'>"
            "<h1>Orion Proxy is running</h1>"
            "<p>Place <code>dashboard.html</code> next to the executable to enable GUI.</p>"
            "</div></body></html>";
        send_response(sock, 200, "text/html", fallback, (int)strlen(fallback));
        return;
    }

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = (char *)malloc(file_size + 1);
    if (!buf) {
        fclose(f);
        send_response(sock, 500, "text/plain", "Out of memory", 13);
        return;
    }

    fread(buf, 1, file_size, f);
    fclose(f);

    send_response(sock, 200, "text/html; charset=utf-8", buf, (int)file_size);
    free(buf);
}

/* ─── API: GET /api/status ─────────────────────────────────────────────── */

static void api_get_status(http_server_t *srv, http_socket_t sock)
{
    char buf[1024];
    time_t now = time(NULL);
    time_t uptime = now - srv->tracker->start_time;
    int online = tracker_online_count(srv->tracker);
    double error_rate = (srv->tracker->total_packets > 0) ?
        (100.0 * srv->tracker->total_crc_errors / srv->tracker->total_packets) : 0.0;

    snprintf(buf, sizeof(buf),
        "{\"uptime\":%lld,\"packets\":%u,\"crc_errors\":%u,"
        "\"error_rate\":\"%.2f%%\",\"devices_online\":%d,\"events\":%u,"
        "\"mode\":\"%s\",\"debug_level\":%d}",
        (long long)uptime,
        srv->tracker->total_packets,
        srv->tracker->total_crc_errors,
        error_rate,
        online,
        srv->tracker->event_count,
        srv->config->master_mode ? "MASTER" : "MONITOR",
        srv->config->debug_level);
    send_json(sock, buf);
}

/* ─── API: GET /api/devices ────────────────────────────────────────────── */

static void api_get_devices(http_server_t *srv, http_socket_t sock)
{
    char buf[HTTP_MAX_RESPONSE];
    int pos = 0;
    int i, first = 1;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "[");

    for (i = 1; i < TRACKER_MAX_DEVICES && pos < (int)sizeof(buf) - 512; ++i) {
        tracked_device_t *d = &srv->tracker->devices[i];
        if (d->packet_count == 0 && !d->online) continue;

        if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        first = 0;

        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"address\":%d,\"online\":%d,\"type\":\"0x%02X\","
            "\"model\":\"%s\",\"category\":\"%s\","
            "\"status1\":\"0x%02X\",\"status1_text\":\"%s\","
            "\"status2\":\"0x%02X\",\"status2_text\":\"%s\","
            "\"packets\":%u,\"last_seen\":%lld,"
            "\"fw_major\":%d,\"fw_minor\":%d,\"zones\":%d}",
            i, d->online, d->device_type,
            orion_device_model(d->device_type),
            orion_device_category_name(orion_device_category(d->device_type)),
            d->last_status1, orion_status_str(d->last_status1),
            d->last_status2, orion_status_str(d->last_status2),
            d->packet_count, (long long)d->last_seen,
            d->fw_major, d->fw_minor,
            orion_device_zone_count(d->device_type));
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
    send_json(sock, buf);
}

/* ─── API: GET /api/events ─────────────────────────────────────────────── */

static void api_get_events(http_server_t *srv, http_socket_t sock)
{
    char buf[HTTP_MAX_RESPONSE];
    int pos = 0;
    int i, first = 1;
    int count;
    int start;

    count = srv->tracker->event_count;
    if (count > TRACKER_MAX_EVENTS) count = TRACKER_MAX_EVENTS;
    if (count > 100) count = 100;

    start = (srv->tracker->event_write_pos - count + TRACKER_MAX_EVENTS) % TRACKER_MAX_EVENTS;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "[");

    for (i = 0; i < count && pos < (int)sizeof(buf) - 256; ++i) {
        int idx = (start + i) % TRACKER_MAX_EVENTS;
        tracked_event_t *e = &srv->tracker->events[idx];
        if (e->id == 0) continue;

        if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        first = 0;

        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"id\":%u,\"device\":%d,\"code\":\"0x%02X\","
            "\"code_text\":\"%s\",\"zone\":%d,\"time\":%lld}",
            e->id, e->device_addr, e->event_code,
            orion_status_str(e->event_code),
            e->zone, (long long)e->timestamp);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
    send_json(sock, buf);
}

/* ─── API: GET /api/config ─────────────────────────────────────────────── */

static void api_get_config(http_server_t *srv, http_socket_t sock)
{
    char buf[2048];
    char escaped_port[128], escaped_log[512];

    json_escape(srv->config->com_port, escaped_port, sizeof(escaped_port));
    json_escape(srv->config->log_file, escaped_log, sizeof(escaped_log));

    snprintf(buf, sizeof(buf),
        "{\"com_port\":\"%s\",\"baud_rate\":%u,\"serial_timeout_ms\":%u,"
        "\"tcp_port\":%u,\"http_port\":%u,"
        "\"encryption_key\":%u,\"device_timeout_sec\":%u,\"scan_interval_sec\":%u,"
        "\"master_mode\":%d,\"auto_scan\":%d,"
        "\"debug_level\":%d,\"log_to_file\":%d,\"log_file\":\"%s\","
        "\"verbose\":%d,\"show_timestamps\":%d}",
        escaped_port, srv->config->baud_rate, srv->config->serial_timeout_ms,
        srv->config->tcp_port, srv->config->http_port,
        srv->config->encryption_key, srv->config->device_timeout_sec,
        srv->config->scan_interval_sec,
        srv->config->master_mode, srv->config->auto_scan,
        srv->config->debug_level, srv->config->log_to_file, escaped_log,
        srv->config->verbose, srv->config->show_timestamps);
    send_json(sock, buf);
}

/* ─── API: GET /api/live_events ────────────────────────────────────────── */

static void api_get_live_events(http_server_t *srv, http_socket_t sock)
{
    char buf[HTTP_MAX_RESPONSE];
    int pos = 0;
    int i, first = 1;
    int count, start;

#ifdef _WIN32
    EnterCriticalSection(&srv->event_lock);
#endif

    count = srv->event_count;
    if (count > HTTP_MAX_EVENTS) count = HTTP_MAX_EVENTS;
    start = (srv->event_write - count + HTTP_MAX_EVENTS) % HTTP_MAX_EVENTS;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "[");

    for (i = 0; i < count && pos < (int)sizeof(buf) - 512; ++i) {
        int idx = (start + i) % HTTP_MAX_EVENTS;
        char escaped[512];

        if (srv->events[idx].data[0] == '\0') continue;
        json_escape(srv->events[idx].data, escaped, sizeof(escaped));

        if (!first) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        first = 0;

        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"time\":\"%s\",\"data\":\"%s\"}",
            srv->events[idx].time_str, escaped);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");

#ifdef _WIN32
    LeaveCriticalSection(&srv->event_lock);
#endif

    send_json(sock, buf);
}

/* ─── API: GET /api/device_types ───────────────────────────────────────── */

static void api_get_device_types(http_socket_t sock)
{
    static const uint8_t types[] = {
        0x01, 0x02, 0x04, 0x05, 0x06, 0x0A, 0x0B, 0x0E, 0x0F, 0x10,
        0x11, 0x13, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x22, 0x24, 0x29, 0x2A, 0x2C, 0x2D, 0x34, 0x37
    };
    char buf[HTTP_MAX_RESPONSE];
    int pos = 0;
    size_t i;

    pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
    for (i = 0; i < sizeof(types)/sizeof(types[0]); ++i) {
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"code\":\"0x%02X\",\"model\":\"%s\",\"category\":\"%s\","
            "\"zones\":%d,\"name\":\"%s\"}",
            types[i],
            orion_device_model(types[i]),
            orion_device_category_name(orion_device_category(types[i])),
            orion_device_zone_count(types[i]),
            orion_device_type_name(types[i]));
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
    send_json(sock, buf);
}

/* ─── API: POST /api/command ───────────────────────────────────────────── */

static void api_post_command(http_server_t *srv, http_socket_t sock,
                             const char *body)
{
    char cmd[256];
    char resp_buf[4096];
    int n;

    if (json_get_string(body, "command", cmd, sizeof(cmd)) != 0) {
        send_json(sock, "{\"error\":\"No command field\"}");
        return;
    }

    /* Execute via tracker format functions for read-only commands */
    if (strcmp(cmd, "PING") == 0) {
        send_json(sock, "{\"response\":\"PONG\"}");
        return;
    }

    if (strcmp(cmd, "GET_STATUS") == 0) {
        api_get_status(srv, sock);
        return;
    }
    if (strcmp(cmd, "GET_DEVICES") == 0) {
        api_get_devices(srv, sock);
        return;
    }
    if (strcmp(cmd, "GET_CONFIG") == 0) {
        api_get_config(srv, sock);
        return;
    }

    /* For other commands, format and return as text */
    n = tracker_format_status(srv->tracker, resp_buf, sizeof(resp_buf));
    {
        char escaped[4096];
        char json_buf[8192];
        json_escape(cmd, escaped, sizeof(escaped));
        snprintf(json_buf, sizeof(json_buf),
            "{\"command\":\"%s\",\"response\":\"Command received\"}", escaped);
        send_json(sock, json_buf);
    }
}

/* ─── API: POST /api/set ───────────────────────────────────────────────── */

static void api_post_set(http_server_t *srv, http_socket_t sock, const char *body)
{
    char key[128], value[256];

    if (json_get_string(body, "key", key, sizeof(key)) != 0 ||
        json_get_string(body, "value", value, sizeof(value)) != 0) {
        send_json(sock, "{\"error\":\"key and value required\"}");
        return;
    }

    if (config_set(srv->config, key, value) == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"ok\":true,\"key\":\"%s\",\"value\":\"%s\"}", key, value);
        send_json(sock, buf);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"error\":\"Unknown key: %s\"}", key);
        send_json(sock, buf);
    }
}

/* ─── API: POST /api/debug_level ───────────────────────────────────────── */

static void api_post_debug_level(http_server_t *srv, http_socket_t sock,
                                  const char *body)
{
    char val[16];
    int level;

    if (json_get_string(body, "level", val, sizeof(val)) != 0) {
        send_json(sock, "{\"error\":\"level required\"}");
        return;
    }

    level = atoi(val);
    if (level >= 0 && level <= 3) {
        char buf[128];
        srv->config->debug_level = level;
        snprintf(buf, sizeof(buf), "{\"ok\":true,\"debug_level\":%d}", level);
        send_json(sock, buf);
    } else {
        send_json(sock, "{\"error\":\"level must be 0-3\"}");
    }
}

/* ─── API: POST /api/save_config ───────────────────────────────────────── */

static void api_post_save_config(http_server_t *srv, http_socket_t sock)
{
    if (config_save_file(srv->config, CONFIG_FILE_NAME) == 0) {
        send_json(sock, "{\"ok\":true,\"message\":\"Config saved\"}");
    } else {
        send_json(sock, "{\"error\":\"Failed to save config\"}");
    }
}

/* ─── API: POST /api/scan_device ───────────────────────────────────────── */

static void api_post_scan_device(http_server_t *srv, http_socket_t sock,
                                  const char *body)
{
    char val[16];
    int addr;
    orion_device_info_t info;

    if (json_get_string(body, "address", val, sizeof(val)) != 0) {
        send_json(sock, "{\"error\":\"address required\"}");
        return;
    }
    addr = atoi(val);

    if (!srv->orion_ctx || addr < 1 || addr > 127) {
        send_json(sock, "{\"error\":\"Invalid address or no RS-485 connection\"}");
        return;
    }

    if (orion_cmd_read_device_info(srv->orion_ctx, (uint8_t)addr, &info) == 0) {
        char buf[512];
        srv->tracker->devices[addr].device_type = info.device_type;
        srv->tracker->devices[addr].fw_major = info.firmware_major;
        srv->tracker->devices[addr].fw_minor = info.firmware_minor;
        srv->tracker->devices[addr].online = 1;
        srv->tracker->devices[addr].last_seen = time(NULL);

        snprintf(buf, sizeof(buf),
            "{\"ok\":true,\"address\":%d,\"type\":\"0x%02X\","
            "\"model\":\"%s\",\"category\":\"%s\","
            "\"fw_major\":%d,\"fw_minor\":%d,\"zones\":%d}",
            addr, info.device_type,
            orion_device_model(info.device_type),
            orion_device_category_name(orion_device_category(info.device_type)),
            info.firmware_major, info.firmware_minor,
            orion_device_zone_count(info.device_type));
        send_json(sock, buf);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"error\":\"No response from device %d\"}", addr);
        send_json(sock, buf);
    }
}

/* ─── Request router ───────────────────────────────────────────────────── */

static void handle_request(http_server_t *srv, http_socket_t sock,
                           http_request_t *req)
{
    /* OPTIONS (CORS preflight) */
    if (strcmp(req->method, "OPTIONS") == 0) {
        send_response(sock, 200, "text/plain", "", 0);
        return;
    }

    /* GET routes */
    if (strcmp(req->method, "GET") == 0) {
        if (strcmp(req->path, "/") == 0 || strcmp(req->path, "/index.html") == 0) {
            serve_dashboard(srv, sock);
        } else if (strcmp(req->path, "/api/status") == 0) {
            api_get_status(srv, sock);
        } else if (strcmp(req->path, "/api/devices") == 0) {
            api_get_devices(srv, sock);
        } else if (strcmp(req->path, "/api/events") == 0) {
            api_get_events(srv, sock);
        } else if (strcmp(req->path, "/api/config") == 0) {
            api_get_config(srv, sock);
        } else if (strcmp(req->path, "/api/live_events") == 0) {
            api_get_live_events(srv, sock);
        } else if (strcmp(req->path, "/api/device_types") == 0) {
            api_get_device_types(sock);
        } else if (strcmp(req->path, "/api/ping") == 0) {
            send_json(sock, "{\"response\":\"PONG\"}");
        } else {
            send_response(sock, 404, "text/plain", "Not Found", 9);
        }
        return;
    }

    /* POST routes */
    if (strcmp(req->method, "POST") == 0) {
        if (strcmp(req->path, "/api/command") == 0) {
            api_post_command(srv, sock, req->body);
        } else if (strcmp(req->path, "/api/set") == 0) {
            api_post_set(srv, sock, req->body);
        } else if (strcmp(req->path, "/api/debug_level") == 0) {
            api_post_debug_level(srv, sock, req->body);
        } else if (strcmp(req->path, "/api/save_config") == 0) {
            api_post_save_config(srv, sock);
        } else if (strcmp(req->path, "/api/scan_device") == 0) {
            api_post_scan_device(srv, sock, req->body);
        } else {
            send_response(sock, 404, "text/plain", "Not Found", 9);
        }
        return;
    }

    send_response(sock, 405, "text/plain", "Method Not Allowed", 18);
}

/* ─── Accept loop (runs in background thread) ──────────────────────────── */

static void http_accept_loop(http_server_t *srv)
{
    while (!(*srv->stop_flag)) {
        struct sockaddr_in client_addr;
        http_socket_t client_sock;
        char recv_buf[HTTP_MAX_REQUEST];
        int received;
        http_request_t req;
        int addr_len = sizeof(client_addr);
        fd_set rfds;
        struct timeval tv;

        /* Use select with timeout so we can check stop_flag */
        FD_ZERO(&rfds);
        FD_SET(srv->listen_sock, &rfds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select((int)srv->listen_sock + 1, &rfds, NULL, NULL, &tv) <= 0) {
            continue;
        }

        client_sock = accept(srv->listen_sock,
                             (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock == HTTP_INVALID_SOCKET) {
            continue;
        }

        /* Read request */
        received = recv(client_sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (received <= 0) {
            http_close_socket(client_sock);
            continue;
        }
        recv_buf[received] = '\0';

        /* If we have Content-Length but haven't received full body, read more */
        if (parse_http_request(recv_buf, received, &req) == 0) {
            if (req.content_length > 0 && req.body_len < req.content_length) {
                int remaining = req.content_length - req.body_len;
                if (remaining > 0 && received + remaining < (int)sizeof(recv_buf)) {
                    int extra = recv(client_sock, recv_buf + received, remaining, 0);
                    if (extra > 0) {
                        received += extra;
                        recv_buf[received] = '\0';
                        parse_http_request(recv_buf, received, &req);
                    }
                }
            }
            handle_request(srv, client_sock, &req);
        } else {
            send_response(client_sock, 400, "text/plain", "Bad Request", 11);
        }

        http_close_socket(client_sock);
    }
}

/* ─── Thread entry points ──────────────────────────────────────────────── */

#ifdef _WIN32
static DWORD WINAPI http_thread_func(LPVOID param)
{
    http_accept_loop((http_server_t *)param);
    return 0;
}
#else
static void *http_thread_func(void *param)
{
    http_accept_loop((http_server_t *)param);
    return NULL;
}
#endif

/* ─── Public API ───────────────────────────────────────────────────────── */

int http_server_init(http_server_t *srv, uint16_t port,
                     device_tracker_t *tracker, orion_ctx_t *ctx,
                     proxy_config_t *config, volatile int *stop_flag)
{
    memset(srv, 0, sizeof(*srv));
    srv->listen_sock = HTTP_INVALID_SOCKET;
    srv->port = port ? port : 8080;
    srv->tracker = tracker;
    srv->orion_ctx = ctx;
    srv->config = config;
    srv->stop_flag = stop_flag;
    srv->event_write = 0;
    srv->event_count = 0;

    /* Try to find dashboard.html near the executable */
    snprintf(srv->html_path, sizeof(srv->html_path), "dashboard.html");

#ifdef _WIN32
    InitializeCriticalSection(&srv->event_lock);
#endif

    return 0;
}

int http_server_start(http_server_t *srv)
{
    struct sockaddr_in addr;
    int opt = 1;

    srv->listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_sock == HTTP_INVALID_SOCKET) {
        printf("[HTTP] Failed to create socket\n");
        return -1;
    }

    setsockopt(srv->listen_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(srv->port);

    if (bind(srv->listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[HTTP] Failed to bind port %d\n", srv->port);
        http_close_socket(srv->listen_sock);
        srv->listen_sock = HTTP_INVALID_SOCKET;
        return -1;
    }

    if (listen(srv->listen_sock, 8) < 0) {
        printf("[HTTP] Failed to listen\n");
        http_close_socket(srv->listen_sock);
        srv->listen_sock = HTTP_INVALID_SOCKET;
        return -1;
    }

    printf("[HTTP] Dashboard at http://localhost:%d\n", srv->port);

    /* Start thread */
#ifdef _WIN32
    srv->thread = CreateThread(NULL, 0, http_thread_func, srv, 0, NULL);
    if (srv->thread == NULL) {
        printf("[HTTP] Failed to create thread\n");
        return -1;
    }
#else
    if (pthread_create(&srv->thread, NULL, http_thread_func, srv) != 0) {
        printf("[HTTP] Failed to create thread\n");
        return -1;
    }
#endif

    return 0;
}

void http_server_push_event(http_server_t *srv, const char *event_text)
{
    time_t now;
    struct tm *tm_info;
    int idx;

    if (!srv || !event_text || event_text[0] == '\0') return;

#ifdef _WIN32
    EnterCriticalSection(&srv->event_lock);
#endif

    now = time(NULL);
    tm_info = localtime(&now);

    idx = srv->event_write % HTTP_MAX_EVENTS;
    strftime(srv->events[idx].time_str, sizeof(srv->events[idx].time_str),
             "%H:%M:%S", tm_info);
    strncpy(srv->events[idx].data, event_text, sizeof(srv->events[idx].data) - 1);
    srv->events[idx].data[sizeof(srv->events[idx].data) - 1] = '\0';

    /* Remove trailing newlines */
    {
        size_t len = strlen(srv->events[idx].data);
        while (len > 0 && (srv->events[idx].data[len-1] == '\n' ||
                           srv->events[idx].data[len-1] == '\r')) {
            srv->events[idx].data[--len] = '\0';
        }
    }

    srv->event_write = (srv->event_write + 1) % HTTP_MAX_EVENTS;
    if (srv->event_count < HTTP_MAX_EVENTS) srv->event_count++;

#ifdef _WIN32
    LeaveCriticalSection(&srv->event_lock);
#endif
}

void http_server_stop(http_server_t *srv)
{
    if (!srv) return;

    if (srv->listen_sock != HTTP_INVALID_SOCKET) {
        http_close_socket(srv->listen_sock);
        srv->listen_sock = HTTP_INVALID_SOCKET;
    }

#ifdef _WIN32
    if (srv->thread) {
        WaitForSingleObject(srv->thread, 3000);
        CloseHandle(srv->thread);
        srv->thread = NULL;
    }
    DeleteCriticalSection(&srv->event_lock);
#else
    pthread_join(srv->thread, NULL);
#endif

    printf("[HTTP] Server stopped\n");
}
