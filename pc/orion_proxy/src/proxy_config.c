#include "proxy_config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─── Defaults ─────────────────────────────────────────────────────────── */

void config_init_defaults(proxy_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

#ifdef _WIN32
    strncpy(cfg->com_port, "COM3", CONFIG_MAX_PORT_NAME - 1);
#else
    strncpy(cfg->com_port, "/dev/ttyUSB0", CONFIG_MAX_PORT_NAME - 1);
#endif

    cfg->baud_rate = 9600;
    cfg->serial_timeout_ms = 100;
    cfg->tcp_port = 9100;
    cfg->http_port = 8080;
    cfg->encryption_key = 0;
    cfg->device_timeout_sec = 30;
    cfg->scan_interval_sec = 0;        /* Disabled by default */
    cfg->master_mode = 0;              /* MONITOR mode */
    cfg->auto_scan = 0;
    cfg->debug_level = 0;
    cfg->log_to_file = 0;
    strncpy(cfg->log_file, "orion_proxy.log", CONFIG_MAX_LOG_FILE - 1);
    cfg->verbose = 0;
    cfg->show_timestamps = 1;
}

/* ─── INI file parser (simple key=value) ───────────────────────────────── */

static void trim_whitespace(char *s)
{
    char *start = s;
    char *end;
    size_t len;

    while (*start == ' ' || *start == '\t') start++;
    len = strlen(start);
    if (len == 0) { s[0] = '\0'; return; }

    end = start + len - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

int config_load_file(proxy_config_t *cfg, const char *filename)
{
    FILE *f;
    char line[512];
    char *eq;
    char key[128], value[256];

    f = fopen(filename, "r");
    if (!f) {
        return -1; /* File not found, use defaults */
    }

    printf("[CONFIG] Loading config from: %s\n", filename);

    while (fgets(line, sizeof(line), f)) {
        trim_whitespace(line);

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            continue;
        }
        /* Skip section headers */
        if (line[0] == '[') {
            continue;
        }

        eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        strncpy(key, line, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';
        strncpy(value, eq + 1, sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';
        trim_whitespace(key);
        trim_whitespace(value);

        if (config_set(cfg, key, value) < 0) {
            printf("[CONFIG] Warning: unknown key '%s'\n", key);
        }
    }

    fclose(f);
    printf("[CONFIG] Configuration loaded successfully\n");
    return 0;
}

int config_save_file(const proxy_config_t *cfg, const char *filename)
{
    FILE *f;

    f = fopen(filename, "w");
    if (!f) {
        printf("[CONFIG] Error: cannot write to %s\n", filename);
        return -1;
    }

    fprintf(f, "# Orion RS-485 Proxy Service Configuration\n");
    fprintf(f, "# Generated automatically — edit as needed\n\n");

    fprintf(f, "[serial]\n");
    fprintf(f, "com_port = %s\n", cfg->com_port);
    fprintf(f, "baud_rate = %u\n", cfg->baud_rate);
    fprintf(f, "serial_timeout_ms = %u\n", cfg->serial_timeout_ms);
    fprintf(f, "\n");

    fprintf(f, "[server]\n");
    fprintf(f, "tcp_port = %u\n", cfg->tcp_port);
    fprintf(f, "http_port = %u\n", cfg->http_port);
    fprintf(f, "\n");

    fprintf(f, "[protocol]\n");
    fprintf(f, "encryption_key = %u\n", cfg->encryption_key);
    fprintf(f, "device_timeout_sec = %u\n", cfg->device_timeout_sec);
    fprintf(f, "scan_interval_sec = %u\n", cfg->scan_interval_sec);
    fprintf(f, "\n");

    fprintf(f, "[mode]\n");
    fprintf(f, "master_mode = %d\n", cfg->master_mode);
    fprintf(f, "auto_scan = %d\n", cfg->auto_scan);
    fprintf(f, "\n");

    fprintf(f, "[debug]\n");
    fprintf(f, "debug_level = %d\n", cfg->debug_level);
    fprintf(f, "log_to_file = %d\n", cfg->log_to_file);
    fprintf(f, "log_file = %s\n", cfg->log_file);
    fprintf(f, "verbose = %d\n", cfg->verbose);
    fprintf(f, "show_timestamps = %d\n", cfg->show_timestamps);
    fprintf(f, "\n");

    fclose(f);
    printf("[CONFIG] Configuration saved to: %s\n", filename);
    return 0;
}

/* ─── Runtime set ──────────────────────────────────────────────────────── */

int config_set(proxy_config_t *cfg, const char *key, const char *value)
{
    if (strcmp(key, "com_port") == 0) {
        strncpy(cfg->com_port, value, CONFIG_MAX_PORT_NAME - 1);
    } else if (strcmp(key, "baud_rate") == 0) {
        cfg->baud_rate = (uint32_t)atoi(value);
    } else if (strcmp(key, "serial_timeout_ms") == 0) {
        cfg->serial_timeout_ms = (uint32_t)atoi(value);
    } else if (strcmp(key, "tcp_port") == 0) {
        cfg->tcp_port = (uint16_t)atoi(value);
    } else if (strcmp(key, "http_port") == 0) {
        cfg->http_port = (uint16_t)atoi(value);
    } else if (strcmp(key, "encryption_key") == 0) {
        cfg->encryption_key = (uint8_t)atoi(value);
    } else if (strcmp(key, "device_timeout_sec") == 0) {
        cfg->device_timeout_sec = (uint32_t)atoi(value);
    } else if (strcmp(key, "scan_interval_sec") == 0) {
        cfg->scan_interval_sec = (uint32_t)atoi(value);
    } else if (strcmp(key, "master_mode") == 0) {
        cfg->master_mode = atoi(value);
    } else if (strcmp(key, "auto_scan") == 0) {
        cfg->auto_scan = atoi(value);
    } else if (strcmp(key, "debug_level") == 0) {
        cfg->debug_level = atoi(value);
    } else if (strcmp(key, "log_to_file") == 0) {
        cfg->log_to_file = atoi(value);
    } else if (strcmp(key, "log_file") == 0) {
        strncpy(cfg->log_file, value, CONFIG_MAX_LOG_FILE - 1);
    } else if (strcmp(key, "verbose") == 0) {
        cfg->verbose = atoi(value);
    } else if (strcmp(key, "show_timestamps") == 0) {
        cfg->show_timestamps = atoi(value);
    } else {
        return -1;
    }
    return 0;
}

/* ─── Command line parsing ─────────────────────────────────────────────── */

int config_apply_args(proxy_config_t *cfg, int argc, char *argv[])
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            strncpy(cfg->com_port, argv[++i], CONFIG_MAX_PORT_NAME - 1);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            cfg->baud_rate = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            cfg->tcp_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            cfg->encryption_key = (uint8_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            cfg->http_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            cfg->verbose = 1;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            cfg->debug_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0) {
            cfg->master_mode = 1;
        } else if (strcmp(argv[i], "--auto-scan") == 0) {
            cfg->auto_scan = 1;
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            cfg->log_to_file = 1;
            strncpy(cfg->log_file, argv[++i], CONFIG_MAX_LOG_FILE - 1);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            /* Config file path — loaded separately */
            ++i;
        } else if (strcmp(argv[i], "--tray") == 0) {
            /* Handled in main.c */
        } else if (strcmp(argv[i], "--background") == 0) {
            /* Handled in main.c */
        } else if (strcmp(argv[i], "--save-config") == 0) {
            /* Handled in main.c */
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            config_print_usage(argv[0]);
            return 1; /* Signal to exit */
        } else {
            printf("Unknown option: %s\n", argv[i]);
            config_print_usage(argv[0]);
            return -1;
        }
    }
    return 0;
}

/* ─── Format ───────────────────────────────────────────────────────────── */

int config_format(const proxy_config_t *cfg, char *buf, size_t buf_size)
{
    return snprintf(buf, buf_size,
        "CONFIG:\n"
        "  com_port = %s\n"
        "  baud_rate = %u\n"
        "  serial_timeout_ms = %u\n"
        "  tcp_port = %u\n"
        "  http_port = %u\n"
        "  encryption_key = %u (0x%02X)\n"
        "  device_timeout_sec = %u\n"
        "  scan_interval_sec = %u\n"
        "  master_mode = %d (%s)\n"
        "  auto_scan = %d\n"
        "  debug_level = %d (%s)\n"
        "  log_to_file = %d\n"
        "  log_file = %s\n"
        "  verbose = %d\n"
        "  show_timestamps = %d\n"
        "END\n",
        cfg->com_port,
        cfg->baud_rate,
        cfg->serial_timeout_ms,
        cfg->tcp_port,
        cfg->http_port,
        cfg->encryption_key, cfg->encryption_key,
        cfg->device_timeout_sec,
        cfg->scan_interval_sec,
        cfg->master_mode, cfg->master_mode ? "MASTER" : "MONITOR",
        cfg->auto_scan,
        cfg->debug_level,
        cfg->debug_level == 0 ? "OFF" :
        cfg->debug_level == 1 ? "EVENTS" :
        cfg->debug_level == 2 ? "PACKETS" : "HEX_DUMP",
        cfg->log_to_file,
        cfg->log_file,
        cfg->verbose,
        cfg->show_timestamps
    );
}

/* ─── Usage ────────────────────────────────────────────────────────────── */

void config_print_usage(const char *prog)
{
    printf(
        "Orion RS-485 Proxy Service v1.0\n"
        "===============================\n"
        "\n"
        "Connects to Orion RS-485 bus via USB adapter, decodes protocol,\n"
        "and serves data to client applications over TCP.\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "Serial options:\n"
        "  -p <port>       COM port (default: COM3 / /dev/ttyUSB0)\n"
        "  -b <baud>       Baud rate (default: 9600)\n"
        "\n"
        "Server options:\n"
        "  -t <port>       TCP listen port (default: 9100)\n"
        "  -w <port>       Web dashboard port (default: 8080)\n"
        "\n"
        "Protocol options:\n"
        "  -k <key>        Orion encryption key 0-255 (default: 0)\n"
        "  -m              Start in MASTER mode (default: MONITOR)\n"
        "  --auto-scan     Scan all addresses on startup\n"
        "\n"
        "Debug options:\n"
        "  -d <level>      Debug level: 0=off, 1=events, 2=packets, 3=hex\n"
        "  -v              Verbose console output\n"
        "  --log <file>    Enable logging to file\n"
        "\n"
        "Config file:\n"
        "  -c <file>       Load config from INI file (default: orion_proxy.ini)\n"
        "  -h, --help      Show this help\n"
        "\n"
        "Config file is loaded first, then command-line args override.\n"
        "Settings can also be changed at runtime via TCP commands.\n"
        "\n"
        "Run mode:\n"
        "  --tray          Run with system tray icon (hides console, Windows)\n"
        "  --background    Run fully headless (no console, no tray)\n"
        "  --save-config   Save current config to INI file and exit\n"
        "\n"
        "Web dashboard:\n"
        "  Open http://localhost:8080 in your browser after starting.\n"
        "  In --tray mode, dashboard opens automatically.\n"
        "\n"
        "Examples:\n"
        "  %s -p COM5 -b 9600 -t 9100\n"
        "  %s -p COM5 --tray\n"
        "  %s -p /dev/ttyUSB0 -v -d 2\n"
        "  %s -c myconfig.ini -m --auto-scan\n"
        "\n"
        "Client connection:\n"
        "  telnet localhost 9100\n"
        "  Then type HELP for available commands.\n"
        "\n",
        prog, prog, prog, prog, prog
    );
}
