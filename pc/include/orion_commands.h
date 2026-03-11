#ifndef ORION_COMMANDS_H
#define ORION_COMMANDS_H

#include <stdint.h>
#include "orion_protocol.h"

/*
 * Orion protocol command codes.
 * Source: reverse-engineered from Bolid device communication and С2000-ПП docs.
 */
#define ORION_CMD_SET_GLOBAL_KEY    0x11
#define ORION_CMD_READ_STATUS       0x57
#define ORION_CMD_READ_STATUS_ARG   0x02
#define ORION_CMD_ARM_ZONE          0x64
#define ORION_CMD_DISARM_ZONE       0x63
#define ORION_CMD_RESET_ALARM       0x65
#define ORION_CMD_READ_EVENT        0x20
#define ORION_CMD_CONFIRM_EVENT     0x21
#define ORION_CMD_READ_DEVICE_TYPE  0x06
#define ORION_CMD_READ_VERSION      0x07

/*
 * Device status codes (from С2000-ПП documentation).
 */
#define ORION_STATUS_ARMED              0x04
#define ORION_STATUS_DISARMED           0x06
#define ORION_STATUS_ALARM              0x09
#define ORION_STATUS_FIRE_ALARM         0x0A
#define ORION_STATUS_ENTRY_DELAY        0x14
#define ORION_STATUS_EXIT_DELAY         0x15
#define ORION_STATUS_TAMPER             0x95  /* Case tamper / Взлом корпуса */
#define ORION_STATUS_POWER_OK           0xC7  /* Power restored / Восстановление питания */
#define ORION_STATUS_POWER_FAIL         0xC8  /* Power failure */
#define ORION_STATUS_CONNECTION_LOST    0xD0
#define ORION_STATUS_CONNECTION_OK      0xD1

/**
 * Device status response.
 */
typedef struct {
    uint8_t status1;
    uint8_t status2;
    uint8_t raw_payload[ORION_MAX_PAYLOAD];
    size_t  raw_length;
} orion_device_status_t;

/**
 * Device information.
 */
typedef struct {
    uint8_t device_type;
    uint8_t firmware_major;
    uint8_t firmware_minor;
} orion_device_info_t;

/**
 * Orion event.
 */
typedef struct {
    uint8_t  event_code;
    uint8_t  zone;
    uint8_t  partition;
    uint16_t timestamp;
} orion_event_t;

/**
 * Set the global encryption key on a device.
 *
 * @param ctx       Protocol context.
 * @param address   Device address (1..127).
 * @param new_key   The new global key to set.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_set_global_key(orion_ctx_t *ctx, uint8_t address, uint8_t new_key);

/**
 * Read the current status of a device (encrypted command).
 *
 * @param ctx       Protocol context.
 * @param address   Device address (1..127).
 * @param status    Output status structure.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_read_status(orion_ctx_t *ctx, uint8_t address,
                          orion_device_status_t *status);

/**
 * Arm a zone/partition on a device.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @param zone      Zone number.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_arm(orion_ctx_t *ctx, uint8_t address, uint8_t zone);

/**
 * Disarm a zone/partition on a device.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @param zone      Zone number.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_disarm(orion_ctx_t *ctx, uint8_t address, uint8_t zone);

/**
 * Reset alarm state on a device.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @param zone      Zone number.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_reset_alarm(orion_ctx_t *ctx, uint8_t address, uint8_t zone);

/**
 * Read the next unconfirmed event from a device.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @param event     Output event structure.
 * @return          0 on success, 1 if no events, -1 on error.
 */
int orion_cmd_read_event(orion_ctx_t *ctx, uint8_t address,
                         orion_event_t *event);

/**
 * Confirm (acknowledge) the last read event.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_confirm_event(orion_ctx_t *ctx, uint8_t address);

/**
 * Read device type and firmware version.
 *
 * @param ctx       Protocol context.
 * @param address   Device address.
 * @param info      Output device info.
 * @return          0 on success, -1 on error.
 */
int orion_cmd_read_device_info(orion_ctx_t *ctx, uint8_t address,
                               orion_device_info_t *info);

/**
 * Get a human-readable string for a status code.
 *
 * @param status_code  The Orion status byte.
 * @return             Static string description (never NULL).
 */
const char *orion_status_str(uint8_t status_code);

#endif /* ORION_COMMANDS_H */
