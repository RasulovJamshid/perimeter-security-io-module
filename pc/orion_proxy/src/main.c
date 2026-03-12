#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include "orion_serial.h"
#include "orion_protocol.h"
#include "orion_sniffer.h"
#include "orion_commands.h"
#include "device_tracker.h"
#include "proxy_server.h"
#include "proxy_config.h"
#include "orion_device_types.h"
#include "http_server.h"

#ifdef _WIN32
#include "tray_icon.h"
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#include <pthread.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

/* ─── Globals ──────────────────────────────────────────────────────────── */

static volatile int  g_stop_flag = 0;
static device_tracker_t g_tracker;
static proxy_server_t   g_server;
static http_server_t    g_http_server;
static orion_ctx_t      g_orion_ctx;
static proxy_config_t   g_config;
static FILE            *g_log_file = NULL;
static int              g_tray_mode = 0;     /* 1 = system tray, hide console */
static int              g_background_mode = 0; /* 1 = no console at all */
#ifdef _WIN32
static tray_icon_t      g_tray_icon;
#endif

#define TIMEOUT_CHECK_SEC   5
#define SCAN_BATCH_DELAY_MS 50

/* ─── Logging ──────────────────────────────────────────────────────────── */

static void proxy_log(int level, const char *fmt, ...)
{
    va_list args;
    char buf[1024];
    char timebuf[32];
    time_t now;
    struct tm *tm_info;

    if (level > g_config.debug_level && !g_config.verbose) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_config.show_timestamps) {
        now = time(NULL);
        tm_info = localtime(&now);
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_info);
        printf("[%s] %s", timebuf, buf);
    } else {
        printf("%s", buf);
    }
    fflush(stdout);

    if (g_log_file) {
        now = time(NULL);
        tm_info = localtime(&now);
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(g_log_file, "[%s] %s", timebuf, buf);
        fflush(g_log_file);
    }
}

/* ─── Signal handler ───────────────────────────────────────────────────── */

static void signal_handler(int sig)
{
    (void)sig;
    printf("\n[MAIN] Shutdown requested (signal %d)...\n", sig);
    fflush(stdout);
    g_stop_flag = 1;
}

/* ─── Debug packet printer ─────────────────────────────────────────────── */

static void debug_print_packet(const orion_packet_t *pkt,
                               const uint8_t *raw, size_t raw_len)
{
    uint8_t addr;
    size_t i;
    const char *model;

    addr = pkt->address;
    if (pkt->is_encrypted && addr >= 0x80) {
        addr = addr - 0x80;
    }

    model = orion_device_model(g_tracker.devices[addr].device_type);

    if (g_config.debug_level >= 1) {
        proxy_log(1, "[PKT] dev=%3d (%s) len=%2d enc=%d",
                  addr, model, pkt->length, pkt->is_encrypted);
    }

    if (g_config.debug_level >= 2) {
        printf("  status=0x%02X(%s)",
               g_tracker.devices[addr].last_status1,
               orion_status_str(g_tracker.devices[addr].last_status1));
    }

    if (g_config.debug_level >= 3) {
        printf("  hex=[");
        for (i = 0; i < raw_len; ++i) {
            printf("%02X", raw[i]);
            if (i < raw_len - 1) printf(" ");
        }
        printf("]");
    }

    if (g_config.debug_level >= 1) {
        printf("\n");
        fflush(stdout);
    }
}

/* ─── Sniffer callback — bridges sniffer to tracker + server ───────────── */

static void sniffer_on_packet(const orion_packet_t *pkt,
                              const uint8_t *raw, size_t raw_len,
                              void *user_data)
{
    char event_buf[512];
    uint8_t addr;
    const char *model;

    (void)user_data;

    addr = pkt->address;
    if (pkt->is_encrypted && addr >= 0x80) {
        addr = addr - 0x80;
    }

    /* Update device tracker */
    tracker_on_packet(&g_tracker, pkt, raw, raw_len);

    model = orion_device_model(g_tracker.devices[addr].device_type);

    /* Broadcast to subscribed clients (enriched with device type) */
    snprintf(event_buf, sizeof(event_buf),
             "PACKET,dev=%d,model=%s,len=%d,enc=%d,"
             "status=0x%02X(%s)\n",
             addr, model, pkt->length, pkt->is_encrypted,
             g_tracker.devices[addr].last_status1,
             orion_status_str(g_tracker.devices[addr].last_status1));
    proxy_server_broadcast_event(&g_server, event_buf);

    /* Push to HTTP dashboard live events */
    http_server_push_event(&g_http_server, event_buf);

    /* Broadcast raw hex to raw subscribers */
    proxy_server_broadcast_raw(&g_server, raw, raw_len, pkt);

    /* Debug output */
    if (g_config.debug_level >= 1) {
        debug_print_packet(pkt, raw, raw_len);
    }

    /* Log to file */
    if (g_log_file) {
        char timebuf[32];
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        size_t i;
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(g_log_file, "[%s] PKT dev=%d model=%s len=%d enc=%d hex=",
                timebuf, addr, model, pkt->length, pkt->is_encrypted);
        for (i = 0; i < raw_len; ++i) {
            fprintf(g_log_file, "%02X", raw[i]);
        }
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
}

/* ─── Sniffer thread ───────────────────────────────────────────────────── */

#ifdef _WIN32
static DWORD WINAPI sniffer_thread_func(LPVOID param)
#else
static void *sniffer_thread_func(void *param)
#endif
{
    orion_sniffer_config_t *config = (orion_sniffer_config_t *)param;
    orion_sniffer_run(config);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ─── Active device scan ───────────────────────────────────────────────── */

static void run_active_scan(void)
{
    int addr;
    orion_device_info_t info;
    int found = 0;

    proxy_log(0, "[SCAN] Starting active scan (addresses 1-%d)...\n",
              ORION_MAX_ADDRESS);

    for (addr = 1; addr <= ORION_MAX_ADDRESS && !g_stop_flag; ++addr) {
        if (orion_cmd_read_device_info(&g_orion_ctx, (uint8_t)addr, &info) == 0) {
            g_tracker.devices[addr].device_type = info.device_type;
            g_tracker.devices[addr].fw_major = info.firmware_major;
            g_tracker.devices[addr].fw_minor = info.firmware_minor;
            g_tracker.devices[addr].online = 1;
            g_tracker.devices[addr].last_seen = time(NULL);

            proxy_log(0, "[SCAN] Found: addr=%d model=%s (%s) fw=%d.%d\n",
                      addr,
                      orion_device_model(info.device_type),
                      orion_device_type_name(info.device_type),
                      info.firmware_major, info.firmware_minor);

            /* Broadcast to clients */
            {
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "DEVICE_FOUND,%d,%s,%s,fw=%d.%d\n",
                         addr,
                         orion_device_model(info.device_type),
                         orion_device_category_name(
                             orion_device_category(info.device_type)),
                         info.firmware_major, info.firmware_minor);
                proxy_server_broadcast_event(&g_server, buf);
            }
            found++;
        }
        SLEEP_MS(SCAN_BATCH_DELAY_MS);
    }

    proxy_log(0, "[SCAN] Complete. Found %d devices.\n", found);
}

/* ─── Main ─────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    orion_serial_config_t serial_cfg;
    orion_sniffer_config_t sniffer_cfg;
    orion_port_t port;
    time_t last_timeout_check;
    time_t last_scan_time;
    const char *config_file = CONFIG_FILE_NAME;
    int i;

#ifdef _WIN32
    HANDLE sniffer_handle;
#else
    pthread_t sniffer_tid;
#endif

    /* ─── Configuration ────────────────────────────────────────────────── */

    config_init_defaults(&g_config);

    /* Check for config file path in args first */
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[i + 1];
            break;
        }
    }

    /* Load config file (defaults used if not found) */
    if (config_load_file(&g_config, config_file) == -1) {
        printf("[CONFIG] No config file found (%s), using defaults.\n", config_file);
        printf("[CONFIG] Run with --save-config to generate one.\n");
    }

    /* Command-line args override config file */
    {
        int ret = config_apply_args(&g_config, argc, argv);
        if (ret != 0) {
            return (ret > 0) ? 0 : 1;
        }
    }

    /* Check for --save-config, --tray, --background */
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--save-config") == 0) {
            config_save_file(&g_config, config_file);
            printf("Config saved. Edit %s and restart.\n", config_file);
            return 0;
        }
        if (strcmp(argv[i], "--tray") == 0) {
            g_tray_mode = 1;
        }
        if (strcmp(argv[i], "--background") == 0) {
            g_background_mode = 1;
        }
    }

    /* Install signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* ─── Tray / background mode ───────────────────────────────────────── */

#ifdef _WIN32
    if (g_tray_mode) {
        /* Enable log to file automatically in tray mode */
        if (!g_config.log_to_file) {
            g_config.log_to_file = 1;
        }
        /* Initialize tray icon (starts message loop thread) */
        tray_icon_init(&g_tray_icon, &g_stop_flag, g_config.http_port);
        /* Hide console window */
        hide_console_window();
        /* Auto-open dashboard in browser */
        open_dashboard_browser(g_config.http_port);
    }
    if (g_background_mode) {
        if (!g_config.log_to_file) {
            g_config.log_to_file = 1;
        }
        FreeConsole();
    }
#endif

    /* ─── Open log file ────────────────────────────────────────────────── */

    if (g_config.log_to_file) {
        g_log_file = fopen(g_config.log_file, "a");
        if (g_log_file) {
            proxy_log(0, "[LOG] Logging to: %s\n", g_config.log_file);
        } else {
            if (!g_background_mode) {
                printf("[WARNING] Cannot open log file: %s\n", g_config.log_file);
            }
        }
    }

    /* ─── Print banner ─────────────────────────────────────────────────── */

    if (!g_background_mode) {
        printf("\n");
        printf("╔══════════════════════════════════════════╗\n");
        printf("║   Orion RS-485 Proxy Service v1.0        ║\n");
        printf("╠══════════════════════════════════════════╣\n");
        printf("║  COM port:  %-28s║\n", g_config.com_port);
        printf("║  Baud rate: %-28u║\n", g_config.baud_rate);
        printf("║  TCP port:  %-28u║\n", g_config.tcp_port);
        printf("║  Dashboard: http://localhost:%-13u║\n", g_config.http_port);
        printf("║  Mode:      %-28s║\n", g_config.master_mode ? "MASTER (active)" : "MONITOR (passive)");
        printf("║  Debug:     %-28s║\n",
               g_config.debug_level == 0 ? "OFF" :
               g_config.debug_level == 1 ? "EVENTS" :
               g_config.debug_level == 2 ? "PACKETS" : "HEX DUMP");
        printf("║  Key:       0x%02X                          ║\n", g_config.encryption_key);
        printf("╚══════════════════════════════════════════╝\n\n");
        fflush(stdout);
    }

    /* ─── Initialize device tracker ────────────────────────────────────── */

    tracker_init(&g_tracker);
    proxy_log(0, "[MAIN] Device tracker initialized (timeout=%us)\n",
              g_config.device_timeout_sec);

    /* ─── Open RS-485 serial port ──────────────────────────────────────── */

    memset(&serial_cfg, 0, sizeof(serial_cfg));
    serial_cfg.port_name = g_config.com_port;
    serial_cfg.baud_rate = g_config.baud_rate;
    serial_cfg.timeout_ms = g_config.serial_timeout_ms;

    port = orion_serial_open(&serial_cfg);
    if (port == ORION_INVALID_PORT) {
        printf("[WARN] Failed to open serial port: %s\n", g_config.com_port);
        printf("[WARN] RS-485 bus will NOT be monitored.\n");
        printf("[WARN] Dashboard still available at http://localhost:%d\n",
               g_config.http_port);
        printf("[WARN] Troubleshooting:\n");
        printf("  1. Is USB-RS485 adapter connected?\n");
        printf("  2. Correct COM port? (use -p COM5 or -p /dev/ttyUSB0)\n");
        printf("  3. Port used by another program?\n");
        printf("  4. Drivers installed? (CH340/FTDI)\n");
#ifdef _WIN32
        printf("  5. Check Device Manager > Ports (COM & LPT)\n");
#else
        printf("  5. Check: ls /dev/ttyUSB* /dev/ttyACM*\n");
        printf("  6. Permission? Try: sudo usermod -aG dialout $USER\n");
#endif
    } else {
        proxy_log(0, "[MAIN] Serial port %s opened at %u baud\n",
                  g_config.com_port, g_config.baud_rate);
    }

    /* ─── Initialize Orion protocol context ────────────────────────────── */

    orion_ctx_init(&g_orion_ctx, port, g_config.encryption_key);
    g_orion_ctx.response_timeout_ms = 500;

    /* ─── Initialize and start TCP server ──────────────────────────────── */

    if (proxy_server_init(&g_server, g_config.tcp_port, &g_tracker,
                          &g_orion_ctx, &g_stop_flag) != 0) {
        printf("[ERROR] Failed to initialize proxy server\n");
        if (port != ORION_INVALID_PORT) orion_serial_close(port);
        tracker_destroy(&g_tracker);
        if (g_log_file) fclose(g_log_file);
        return 1;
    }

    /* Pass config pointer to server for runtime config commands */
    g_server.config = &g_config;
    g_server.verbose = g_config.verbose;

    if (proxy_server_start(&g_server) != 0) {
        printf("[ERROR] Failed to start TCP server on port %d\n", g_config.tcp_port);
        if (port != ORION_INVALID_PORT) orion_serial_close(port);
        tracker_destroy(&g_tracker);
        if (g_log_file) fclose(g_log_file);
        return 1;
    }

    /* ─── Start embedded HTTP dashboard server ─────────────────────────── */

    if (http_server_init(&g_http_server, g_config.http_port,
                         &g_tracker, &g_orion_ctx,
                         &g_config, &g_stop_flag) == 0) {
        if (http_server_start(&g_http_server) != 0) {
            printf("[WARN] HTTP dashboard failed to start on port %d\n",
                   g_config.http_port);
        }
    }

    /* ─── Start sniffer thread (only if serial port is open) ───────────── */

    if (port != ORION_INVALID_PORT) {
        memset(&sniffer_cfg, 0, sizeof(sniffer_cfg));
        sniffer_cfg.port = port;
        sniffer_cfg.known_key = g_config.encryption_key;
        sniffer_cfg.on_packet = sniffer_on_packet;
        sniffer_cfg.user_data = NULL;
        sniffer_cfg.verbose = (g_config.debug_level >= 2) ? g_config.debug_level - 1 : 0;
        sniffer_cfg.stop_flag = &g_stop_flag;

#ifdef _WIN32
        sniffer_handle = CreateThread(NULL, 0, sniffer_thread_func,
                                      &sniffer_cfg, 0, NULL);
        if (sniffer_handle == NULL) {
            printf("[ERROR] Failed to create sniffer thread\n");
        }
#else
        if (pthread_create(&sniffer_tid, NULL, sniffer_thread_func,
                           &sniffer_cfg) != 0) {
            printf("[ERROR] Failed to create sniffer thread\n");
        }
#endif

        proxy_log(0, "[MAIN] Sniffer thread started (%s mode)\n",
                  g_config.master_mode ? "MASTER" : "MONITOR");
    } else {
        proxy_log(0, "[MAIN] No serial port — running dashboard-only mode\n");
    }

    /* ─── Auto-scan on startup ─────────────────────────────────────────── */

    if (g_config.auto_scan && g_config.master_mode) {
        run_active_scan();
    } else if (g_config.auto_scan && !g_config.master_mode) {
        proxy_log(0, "[MAIN] Auto-scan skipped (requires MASTER mode, use -m)\n");
    }

    proxy_log(0, "[MAIN] Ready. Press Ctrl+C to stop.\n\n");
    fflush(stdout);

    /* ─── Main loop ────────────────────────────────────────────────────── */

    last_timeout_check = time(NULL);
    last_scan_time = time(NULL);

    while (!g_stop_flag) {
        /* Poll TCP server for client connections and commands */
        proxy_server_poll(&g_server);

        /* Periodic device timeout check */
        if (time(NULL) - last_timeout_check >= TIMEOUT_CHECK_SEC) {
            tracker_check_timeouts(&g_tracker);
            last_timeout_check = time(NULL);

            /* Console stats */
            if (g_config.verbose && !g_background_mode) {
                proxy_log(0, "[STATS] Pkts:%u CRC_err:%u Online:%d Clients:%d\n",
                          g_tracker.total_packets,
                          g_tracker.total_crc_errors,
                          tracker_online_count(&g_tracker),
                          g_server.client_count);
            }

#ifdef _WIN32
            /* Update tray icon tooltip */
            if (g_tray_mode) {
                tray_icon_update(&g_tray_icon,
                                 port != ORION_INVALID_PORT,
                                 tracker_online_count(&g_tracker),
                                 g_tracker.total_packets);
            }
#endif
        }

        /* Periodic active scan (if configured and in MASTER mode) */
        if (g_config.scan_interval_sec > 0 && g_config.master_mode) {
            if (time(NULL) - last_scan_time >= (time_t)g_config.scan_interval_sec) {
                run_active_scan();
                last_scan_time = time(NULL);
            }
        }

        SLEEP_MS(10);
    }

    /* ─── Cleanup ──────────────────────────────────────────────────────── */

    proxy_log(0, "\n[MAIN] Stopping...\n");

    if (port != ORION_INVALID_PORT) {
#ifdef _WIN32
        if (sniffer_handle) {
            WaitForSingleObject(sniffer_handle, 3000);
            CloseHandle(sniffer_handle);
        }
#else
        pthread_join(sniffer_tid, NULL);
#endif
    }

    proxy_server_shutdown(&g_server);
    http_server_stop(&g_http_server);
#ifdef _WIN32
    if (g_tray_mode) {
        tray_icon_destroy(&g_tray_icon);
    }
#endif
    if (port != ORION_INVALID_PORT) {
        orion_serial_close(port);
        proxy_log(0, "[MAIN] Serial port closed\n");
    }
    tracker_destroy(&g_tracker);

    /* Final stats */
    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Final Statistics                  ║\n");
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Packets received: %-21u║\n", g_tracker.total_packets);
    printf("║  CRC errors:       %-21u║\n", g_tracker.total_crc_errors);
    printf("║  Events logged:    %-21u║\n", g_tracker.event_count);
    printf("║  Devices seen:     %-21d║\n", tracker_online_count(&g_tracker));
    printf("╚══════════════════════════════════════════╝\n");

    /* Save config on exit */
    config_save_file(&g_config, config_file);

    if (g_log_file) {
        fprintf(g_log_file, "--- Proxy stopped ---\n");
        fclose(g_log_file);
    }

    return 0;
}
