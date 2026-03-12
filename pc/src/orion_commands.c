#include "orion_commands.h"
#include <string.h>
#include <stdio.h>

int orion_cmd_set_global_key(orion_ctx_t *ctx, uint8_t address, uint8_t new_key)
{
    if (!ctx) return -1;

    /*
     * Set global key command (unencrypted):
     *   [address] [length] [key_xor] [0x11] [new_key] [new_key] [crc]
     *
     * key_xor = old_global_key ^ message_key
     * When keys are the same: key_xor = 0x00
     */
    uint8_t cmd_data[4];
    cmd_data[0] = ctx->global_key ^ ctx->message_key;
    cmd_data[1] = ORION_CMD_SET_GLOBAL_KEY;
    cmd_data[2] = new_key;
    cmd_data[3] = new_key; /* repeated for safety */

    orion_packet_t request, response;
    int ret = orion_packet_build(&request, address, cmd_data, sizeof(cmd_data));
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    /* Update context with new key */
    ctx->global_key = new_key;
    ctx->message_key = new_key;

    printf("[orion] Global key set to 0x%02X on device %d\n", new_key, address);
    return 0;
}

int orion_cmd_read_status(orion_ctx_t *ctx, uint8_t address,
                          orion_device_status_t *status)
{
    if (!ctx || !status) return -1;

    /*
     * Read status (encrypted):
     *   Plaintext command bytes: [0x57, 0x02]
     *   These get XOR'd with message_key during packet construction.
     */
    uint8_t cmd_data[2];
    cmd_data[0] = ORION_CMD_READ_STATUS;
    cmd_data[1] = ORION_CMD_READ_STATUS_ARG;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    /* Decrypt the response payload */
    orion_packet_decrypt_payload(&response, ctx->message_key);

    /* Extract status bytes from decrypted payload */
    memset(status, 0, sizeof(*status));
    size_t payload_len = response.length - 1;
    memcpy(status->raw_payload, response.payload, payload_len);
    status->raw_length = payload_len;

    /*
     * Response layout (after decryption, offsets within payload):
     *   [0] key_xor (GLOBAL_KEY ^ MESSAGE_KEY, not decrypted)
     *   [1] echo byte (0x02 seen)
     *   [2] message_key echo
     *   [3] unknown (0x04 seen)
     *   [4] unknown (0x03 seen)
     *   [5] STATUS_1
     *   [6] STATUS_2
     *   [7] unknown (0xC8 seen)
     *
     * Verified against Habr Bolid Orion reverse-engineering article.
     * Note: offsets may vary slightly by device type and firmware.
     */
    if (payload_len >= 7) {
        status->status1 = response.payload[5];
        status->status2 = response.payload[6];
    } else if (payload_len >= 6) {
        status->status1 = response.payload[4];
        status->status2 = response.payload[5];
    }

    printf("[orion] Device %d status: 0x%02X (%s), 0x%02X (%s)\n",
           address & 0x7F,
           status->status1, orion_status_str(status->status1),
           status->status2, orion_status_str(status->status2));

    return 0;
}

int orion_cmd_arm(orion_ctx_t *ctx, uint8_t address, uint8_t zone)
{
    if (!ctx) return -1;

    uint8_t cmd_data[2];
    cmd_data[0] = ORION_CMD_ARM_ZONE;
    cmd_data[1] = zone;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    printf("[orion] Armed zone %d on device %d\n", zone, address);
    return 0;
}

int orion_cmd_disarm(orion_ctx_t *ctx, uint8_t address, uint8_t zone)
{
    if (!ctx) return -1;

    uint8_t cmd_data[2];
    cmd_data[0] = ORION_CMD_DISARM_ZONE;
    cmd_data[1] = zone;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    printf("[orion] Disarmed zone %d on device %d\n", zone, address);
    return 0;
}

int orion_cmd_reset_alarm(orion_ctx_t *ctx, uint8_t address, uint8_t zone)
{
    if (!ctx) return -1;

    uint8_t cmd_data[2];
    cmd_data[0] = ORION_CMD_RESET_ALARM;
    cmd_data[1] = zone;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    printf("[orion] Alarm reset on zone %d, device %d\n", zone, address);
    return 0;
}

int orion_cmd_read_event(orion_ctx_t *ctx, uint8_t address,
                         orion_event_t *event)
{
    if (!ctx || !event) return -1;

    uint8_t cmd_data[1];
    cmd_data[0] = ORION_CMD_READ_EVENT;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    orion_packet_decrypt_payload(&response, ctx->message_key);

    size_t payload_len = response.length - 1;
    if (payload_len < 4) {
        return 1; /* No events available */
    }

    memset(event, 0, sizeof(*event));
    event->event_code = response.payload[1];
    event->zone       = response.payload[2];
    event->partition  = response.payload[3];

    return 0;
}

int orion_cmd_confirm_event(orion_ctx_t *ctx, uint8_t address)
{
    if (!ctx) return -1;

    uint8_t cmd_data[1];
    cmd_data[0] = ORION_CMD_CONFIRM_EVENT;

    orion_packet_t request, response;
    int ret = orion_packet_build_encrypted(&request, address, cmd_data, sizeof(cmd_data),
                                           ctx->global_key, ctx->message_key);
    if (ret < 0) return -1;

    return orion_transact(ctx, &request, &response);
}

int orion_cmd_read_device_info(orion_ctx_t *ctx, uint8_t address,
                               orion_device_info_t *info)
{
    if (!ctx || !info) return -1;

    uint8_t cmd_data[1];
    cmd_data[0] = ORION_CMD_READ_DEVICE_TYPE;

    orion_packet_t request, response;
    int ret = orion_packet_build(&request, address, cmd_data, sizeof(cmd_data));
    if (ret < 0) return -1;

    ret = orion_transact(ctx, &request, &response);
    if (ret < 0) return -1;

    memset(info, 0, sizeof(*info));
    size_t payload_len = response.length - 1;
    if (payload_len >= 1) info->device_type    = response.payload[0];
    if (payload_len >= 2) info->firmware_major  = response.payload[1];
    if (payload_len >= 3) info->firmware_minor  = response.payload[2];

    printf("[orion] Device %d: type=0x%02X, firmware=%d.%d\n",
           address, info->device_type, info->firmware_major, info->firmware_minor);
    return 0;
}

const char *orion_status_str(uint8_t status_code)
{
    switch (status_code) {
        /* Alarm & security states */
        case 0x01: return "Short circuit (loop)";
        case 0x02: return "Open circuit (loop)";
        case 0x03: return "Alarm (loop triggered)";
        case ORION_STATUS_ARMED:           return "Armed";
        case 0x05: return "Arming in progress";
        case ORION_STATUS_DISARMED:        return "Disarmed";
        case 0x07: return "Arm failed";
        case 0x08: return "Arm failed (zone not ready)";
        case ORION_STATUS_ALARM:           return "ALARM";
        case ORION_STATUS_FIRE_ALARM:      return "FIRE ALARM";
        case 0x0B: return "Fire alarm (manual)";
        case 0x0C: return "Fire pre-alarm";
        case 0x0D: return "Attention (smoke detected)";
        case 0x0E: return "Sensor fault";
        case 0x0F: return "Relay ON";
        case 0x10: return "Relay OFF";

        /* Entry/exit delays */
        case ORION_STATUS_ENTRY_DELAY:     return "Entry delay";
        case ORION_STATUS_EXIT_DELAY:      return "Exit delay";

        /* Tamper & power */
        case ORION_STATUS_TAMPER:          return "Case tamper";
        case 0x96: return "Case tamper restored";
        case ORION_STATUS_POWER_OK:        return "Power restored";
        case ORION_STATUS_POWER_FAIL:      return "Power failure";
        case 0xC9: return "Battery low";
        case 0xCA: return "Battery OK";

        /* Connection */
        case ORION_STATUS_CONNECTION_LOST: return "Connection lost";
        case ORION_STATUS_CONNECTION_OK:   return "Connection restored";

        /* Access control */
        case 0x20: return "Access granted";
        case 0x21: return "Access denied";
        case 0x22: return "Door opened";
        case 0x23: return "Door closed";
        case 0x24: return "Door forced open";
        case 0x25: return "Door held open too long";

        default: return "Unknown status";
    }
}
