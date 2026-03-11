#include "orion_protocol.h"
#include "orion_crc.h"
#include <string.h>
#include <stdio.h>

void orion_ctx_init(orion_ctx_t *ctx, orion_port_t port, uint8_t global_key)
{
    if (!ctx) return;
    ctx->port = port;
    ctx->global_key = global_key;
    ctx->message_key = global_key;
    ctx->response_timeout_ms = 500;
}

int orion_packet_build(orion_packet_t *pkt, uint8_t address,
                       const uint8_t *cmd_data, size_t cmd_len)
{
    if (!pkt || !cmd_data || address == 0 || address > ORION_MAX_ADDRESS) {
        return -1;
    }
    if (cmd_len > ORION_MAX_PAYLOAD) {
        return -1;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->address = address;
    pkt->is_encrypted = 0;

    /*
     * Packet layout (unencrypted):
     *   [address] [length] [payload...] [crc]
     * length = number of bytes following the address byte (payload + crc)
     */
    memcpy(pkt->payload, cmd_data, cmd_len);
    pkt->length = (uint8_t)(cmd_len + 1); /* +1 for CRC byte */

    /* Calculate CRC over: address, length, payload */
    uint8_t crc_buf[ORION_MAX_PACKET];
    crc_buf[0] = pkt->address;
    crc_buf[1] = pkt->length;
    memcpy(&crc_buf[2], pkt->payload, cmd_len);
    pkt->crc = orion_crc8(crc_buf, 2 + cmd_len);

    return (int)(2 + cmd_len + 1); /* address + length + payload + crc */
}

int orion_packet_build_encrypted(orion_packet_t *pkt, uint8_t address,
                                 const uint8_t *cmd_data, size_t cmd_len,
                                 uint8_t global_key, uint8_t message_key)
{
    if (!pkt || !cmd_data || address == 0 || address > ORION_MAX_ADDRESS) {
        return -1;
    }

    /* Encrypted payload:
     *   [key_xor] [encrypted_cmd...] [msg_key_padding...]
     * key_xor = global_key ^ message_key
     * Each command byte is XOR'd with message_key
     * Padding bytes are message_key itself
     */
    size_t enc_payload_len = 1 + cmd_len + 3; /* key_xor + encrypted_cmd + 3 padding */
    size_t idx;
    size_t i;
    if (enc_payload_len > ORION_MAX_PAYLOAD) {
        return -1;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->address = address + ORION_ENCRYPT_OFFSET;
    pkt->is_encrypted = 1;

    /* Build encrypted payload */
    idx = 0;
    pkt->payload[idx++] = global_key ^ message_key;

    for (i = 0; i < cmd_len; ++i) {
        pkt->payload[idx++] = cmd_data[i] ^ message_key;
    }

    /* Padding with message_key */
    pkt->payload[idx++] = message_key;
    pkt->payload[idx++] = message_key;
    pkt->payload[idx++] = message_key;

    pkt->length = (uint8_t)(idx + 1); /* +1 for CRC */

    /* Calculate CRC over all bytes except CRC itself */
    uint8_t crc_buf[ORION_MAX_PACKET];
    crc_buf[0] = pkt->address;
    crc_buf[1] = pkt->length;
    memcpy(&crc_buf[2], pkt->payload, idx);
    pkt->crc = orion_crc8(crc_buf, 2 + idx);

    return (int)(2 + idx + 1);
}

int orion_packet_serialize(const orion_packet_t *pkt, uint8_t *buf)
{
    if (!pkt || !buf) return -1;

    size_t payload_len = pkt->length - 1; /* length includes the CRC byte */
    buf[0] = pkt->address;
    buf[1] = pkt->length;
    memcpy(&buf[2], pkt->payload, payload_len);
    buf[2 + payload_len] = pkt->crc;

    return (int)(2 + payload_len + 1);
}

int orion_packet_parse(const uint8_t *buf, size_t len, orion_packet_t *pkt)
{
    if (!buf || !pkt || len < 3) {
        return -1;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->address = buf[0];
    pkt->length  = buf[1];
    pkt->is_encrypted = (pkt->address > ORION_MAX_ADDRESS) ? 1 : 0;

    /* length byte = payload bytes + CRC byte */
    size_t payload_len = pkt->length - 1;
    if (2 + payload_len + 1 > len) {
        return -1; /* buffer too short */
    }

    memcpy(pkt->payload, &buf[2], payload_len);
    pkt->crc = buf[2 + payload_len];

    /* Verify CRC */
    uint8_t expected_crc = orion_crc8(buf, 2 + payload_len);
    if (expected_crc != pkt->crc) {
        fprintf(stderr, "[orion] CRC mismatch: expected 0x%02X, got 0x%02X\n",
                expected_crc, pkt->crc);
        return -1;
    }

    return 0;
}

void orion_packet_decrypt_payload(orion_packet_t *pkt, uint8_t message_key)
{
    if (!pkt || !pkt->is_encrypted) return;

    size_t payload_len = pkt->length - 1;
    size_t i;
    /* Skip byte 0 (key_xor), decrypt the rest */
    for (i = 1; i < payload_len; ++i) {
        pkt->payload[i] ^= message_key;
    }
}

int orion_transact(orion_ctx_t *ctx, const orion_packet_t *request,
                   orion_packet_t *response)
{
    if (!ctx || !request || !response) return -1;

    /* Serialize the request */
    uint8_t tx_buf[ORION_MAX_PACKET];
    int tx_len = orion_packet_serialize(request, tx_buf);
    if (tx_len < 0) return -1;

    /* Flush before sending */
    orion_serial_flush(ctx->port);

    /* Send */
    int written = orion_serial_write(ctx->port, tx_buf, (size_t)tx_len);
    if (written != tx_len) {
        fprintf(stderr, "[orion] Write failed: sent %d of %d bytes\n", written, tx_len);
        return -1;
    }

    /* Receive response — first read address + length (2 bytes) */
    uint8_t rx_buf[ORION_MAX_PACKET];
    int n = orion_serial_read(ctx->port, rx_buf, 2);
    if (n < 2) {
        fprintf(stderr, "[orion] No response from device (got %d bytes)\n", n);
        return -1;
    }

    /* Now read the remaining bytes: payload + CRC */
    size_t remaining = rx_buf[1]; /* length byte = payload + crc */
    n = orion_serial_read(ctx->port, &rx_buf[2], remaining);
    if (n < (int)remaining) {
        fprintf(stderr, "[orion] Incomplete response: got %d of %u bytes\n",
                n, (unsigned)remaining);
        return -1;
    }

    /* Parse */
    return orion_packet_parse(rx_buf, 2 + remaining, response);
}
