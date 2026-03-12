#ifndef PROXY_CONFIG_H
#define PROXY_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define CONFIG_FILE_NAME        "orion_proxy.ini"
#define CONFIG_MAX_PORT_NAME    64
#define CONFIG_MAX_LOG_FILE     256

/**
 * Proxy configuration — all settings in one place.
 * Can be loaded from file, command line, or changed at runtime via TCP.
 */
typedef struct {
    /* Serial port */
    char     com_port[CONFIG_MAX_PORT_NAME];
    uint32_t baud_rate;
    uint32_t serial_timeout_ms;

    /* TCP server */
    uint16_t tcp_port;
    uint16_t http_port;                 /* HTTP dashboard port (default 8080) */

    /* Orion protocol */
    uint8_t  encryption_key;
    uint32_t device_timeout_sec;       /* Seconds before device marked offline */
    uint32_t scan_interval_sec;        /* Active scan interval (0 = disabled) */

    /* Operation mode */
    int      master_mode;              /* 0 = MONITOR (passive), 1 = MASTER (active) */
    int      auto_scan;                /* 1 = automatically scan for devices on start */

    /* Debug */
    int      debug_level;              /* 0=off, 1=events, 2=packets, 3=hex dumps */
    int      log_to_file;              /* 1 = log to file */
    char     log_file[CONFIG_MAX_LOG_FILE];

    /* Display */
    int      verbose;                  /* Console verbosity */
    int      show_timestamps;          /* 1 = prefix lines with timestamp */
} proxy_config_t;

/**
 * Initialize config with default values.
 */
void config_init_defaults(proxy_config_t *cfg);

/**
 * Load configuration from INI file.
 * @return 0 on success, -1 if file not found (defaults used), -2 on parse error.
 */
int config_load_file(proxy_config_t *cfg, const char *filename);

/**
 * Save current configuration to INI file.
 * @return 0 on success, -1 on error.
 */
int config_save_file(const proxy_config_t *cfg, const char *filename);

/**
 * Apply command-line arguments over current config.
 * @return 0 on success, -1 on error (prints usage).
 */
int config_apply_args(proxy_config_t *cfg, int argc, char *argv[]);

/**
 * Format current config as text for display/sending to client.
 * @return Number of bytes written.
 */
int config_format(const proxy_config_t *cfg, char *buf, size_t buf_size);

/**
 * Apply a single "key=value" setting at runtime.
 * @return 0 on success, -1 if key unknown, -2 if value invalid.
 */
int config_set(proxy_config_t *cfg, const char *key, const char *value);

/**
 * Print usage/help to stdout.
 */
void config_print_usage(const char *prog);

#endif /* PROXY_CONFIG_H */
