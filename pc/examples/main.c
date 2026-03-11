#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "orion_serial.h"
#include "orion_protocol.h"
#include "orion_commands.h"
#include "orion_sniffer.h"

static volatile int g_stop_flag = 0;

static void signal_handler(int sig)
{
    (void)sig;
    g_stop_flag = 1;
}

static void print_usage(const char *prog)
{
    printf("Bolid Orion Protocol Integration\n");
    printf("Usage: %s <COM_PORT> <DEVICE_ADDRESS> <COMMAND> [args...]\n\n", prog);
    printf("Commands:\n");
    printf("  info                   Read device type and firmware version\n");
    printf("  status                 Read device status\n");
    printf("  setkey <hex_key>       Set global encryption key (e.g. 0xBA)\n");
    printf("  arm <zone>             Arm a zone\n");
    printf("  disarm <zone>          Disarm a zone\n");
    printf("  reset <zone>           Reset alarm on a zone\n");
    printf("  events                 Read and display pending events\n");
    printf("  scan                   Scan for devices on the bus (addr 1..127)\n");
    printf("  sniff                  Passive sniffer (listen-only, safe with desktop app)\n");
    printf("\nExamples:\n");
    printf("  %s COM3 1 info\n", prog);
    printf("  %s COM3 3 status\n", prog);
    printf("  %s COM3 3 setkey 0xBA\n", prog);
    printf("  %s COM3 3 arm 1\n", prog);
    printf("  %s /dev/ttyUSB0 1 scan\n", prog);
    printf("  %s COM3 0 sniff\n", prog);
}

static int cmd_info(orion_ctx_t *ctx, uint8_t address)
{
    orion_device_info_t info;
    int ret = orion_cmd_read_device_info(ctx, address, &info);
    if (ret == 0) {
        printf("Device type : 0x%02X\n", info.device_type);
        printf("Firmware    : %d.%d\n", info.firmware_major, info.firmware_minor);
    }
    return ret;
}

static int cmd_status(orion_ctx_t *ctx, uint8_t address)
{
    orion_device_status_t status;
    size_t i;
    int ret = orion_cmd_read_status(ctx, address, &status);
    if (ret == 0) {
        printf("Status 1: 0x%02X - %s\n", status.status1, orion_status_str(status.status1));
        printf("Status 2: 0x%02X - %s\n", status.status2, orion_status_str(status.status2));
        printf("Raw payload (%u bytes): ", (unsigned)status.raw_length);
        for (i = 0; i < status.raw_length; ++i) {
            printf("%02X ", status.raw_payload[i]);
        }
        printf("\n");
    }
    return ret;
}

static int cmd_events(orion_ctx_t *ctx, uint8_t address)
{
    int count = 0;
    int ret;
    orion_event_t event;
    printf("Reading events from device %d...\n", address);

    while (1) {
        ret = orion_cmd_read_event(ctx, address, &event);
        if (ret == 1) {
            printf("No more events.\n");
            break;
        }
        if (ret < 0) {
            printf("Error reading event.\n");
            return -1;
        }

        printf("Event #%d: code=0x%02X zone=%d partition=%d\n",
               ++count, event.event_code, event.zone, event.partition);

        /* Confirm the event */
        orion_cmd_confirm_event(ctx, address);
    }

    printf("Total events read: %d\n", count);
    return 0;
}

static int cmd_scan(orion_ctx_t *ctx)
{
    int found = 0;
    uint8_t addr;
    orion_device_info_t info;
    int ret;
    printf("Scanning Orion bus for devices (addresses 1..127)...\n");

    for (addr = 1; addr <= 127; ++addr) {
        ret = orion_cmd_read_device_info(ctx, addr, &info);
        if (ret == 0) {
            printf("  [%3d] type=0x%02X firmware=%d.%d\n",
                   addr, info.device_type, info.firmware_major, info.firmware_minor);
            found++;
        }
    }

    printf("Scan complete. Found %d device(s).\n", found);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *port_name;
    uint8_t address;
    const char *command;
    orion_serial_config_t serial_cfg;
    orion_port_t port;
    orion_ctx_t ctx;
    int result = -1;

    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    port_name = argv[1];
    address = (uint8_t)atoi(argv[2]);
    command = argv[3];

    /* Open serial port with Orion defaults (9600 8N1) */
    memset(&serial_cfg, 0, sizeof(serial_cfg));
    serial_cfg.port_name  = port_name;
    serial_cfg.baud_rate  = 9600;
    serial_cfg.timeout_ms = 500;

    port = orion_serial_open(&serial_cfg);
    if (port == ORION_INVALID_PORT) {
        fprintf(stderr, "Error: Could not open serial port '%s'\n", port_name);
        return 1;
    }

    printf("Connected to %s\n", port_name);

    /* Initialize protocol context (default key = 0x00) */
    orion_ctx_init(&ctx, port, 0x00);

    if (strcmp(command, "info") == 0) {
        result = cmd_info(&ctx, address);
    }
    else if (strcmp(command, "status") == 0) {
        result = cmd_status(&ctx, address);
    }
    else if (strcmp(command, "setkey") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: setkey requires a hex key argument (e.g. 0xBA)\n");
            result = 1;
        } else {
            uint8_t new_key = (uint8_t)strtol(argv[4], NULL, 0);
            result = orion_cmd_set_global_key(&ctx, address, new_key);
        }
    }
    else if (strcmp(command, "arm") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: arm requires a zone number\n");
            result = 1;
        } else {
            uint8_t zone = (uint8_t)atoi(argv[4]);
            result = orion_cmd_arm(&ctx, address, zone);
        }
    }
    else if (strcmp(command, "disarm") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: disarm requires a zone number\n");
            result = 1;
        } else {
            uint8_t zone = (uint8_t)atoi(argv[4]);
            result = orion_cmd_disarm(&ctx, address, zone);
        }
    }
    else if (strcmp(command, "reset") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: reset requires a zone number\n");
            result = 1;
        } else {
            uint8_t zone = (uint8_t)atoi(argv[4]);
            result = orion_cmd_reset_alarm(&ctx, address, zone);
        }
    }
    else if (strcmp(command, "events") == 0) {
        result = cmd_events(&ctx, address);
    }
    else if (strcmp(command, "scan") == 0) {
        result = cmd_scan(&ctx);
    }
    else if (strcmp(command, "sniff") == 0) {
        orion_sniffer_config_t sniff_cfg;
        memset(&sniff_cfg, 0, sizeof(sniff_cfg));
        sniff_cfg.port       = port;
        sniff_cfg.known_key  = ctx.global_key;
        sniff_cfg.on_packet  = NULL;
        sniff_cfg.user_data  = NULL;
        sniff_cfg.verbose    = 2;
        sniff_cfg.stop_flag  = &g_stop_flag;

        signal(SIGINT, signal_handler);
        result = orion_sniffer_run(&sniff_cfg);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        result = 1;
    }

    orion_serial_close(port);

    if (result == 0) {
        printf("OK\n");
    } else {
        printf("FAILED (code %d)\n", result);
    }

    return (result == 0) ? 0 : 1;
}
