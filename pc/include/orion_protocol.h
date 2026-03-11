#ifndef ORION_PROTOCOL_H
#define ORION_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "orion_serial.h"

/* Maximum payload size for an Orion packet (excluding address, length, CRC) */
#define ORION_MAX_PAYLOAD   252
/* Maximum total packet size */
#define ORION_MAX_PACKET    256
/* Address offset for encrypted messages */
#define ORION_ENCRYPT_OFFSET 0x80
/* Maximum device address */
#define ORION_MAX_ADDRESS    127

/**
 * Orion protocol context — holds connection state and encryption keys.
 */
typedef struct {
    orion_port_t port;
    uint8_t      global_key;     /* Shared key set on each device (default 0x00) */
    uint8_t      message_key;    /* Per-message key (often same as global_key) */
    uint32_t     response_timeout_ms;
} orion_ctx_t;

/**
 * Raw Orion packet structure.
 */
typedef struct {
    uint8_t address;            /* Device address (1..127) */
    uint8_t length;             /* Number of bytes that follow (excl. address byte) */
    uint8_t payload[ORION_MAX_PAYLOAD];
    uint8_t crc;
    uint8_t is_encrypted;       /* 1 if this packet uses encryption */
} orion_packet_t;

/**
 * Initialize an Orion protocol context.
 *
 * @param ctx         Context to initialize.
 * @param port        Open serial port handle.
 * @param global_key  The global key configured on target devices.
 */
void orion_ctx_init(orion_ctx_t *ctx, orion_port_t port, uint8_t global_key);

/**
 * Build a raw (unencrypted) Orion packet.
 *
 * @param pkt         Output packet.
 * @param address     Device address (1..127).
 * @param cmd_data    Command + data bytes.
 * @param cmd_len     Length of cmd_data.
 * @return            Total packet size in bytes, or -1 on error.
 */
int orion_packet_build(orion_packet_t *pkt, uint8_t address,
                       const uint8_t *cmd_data, size_t cmd_len);

/**
 * Build an encrypted Orion packet.
 *
 * @param pkt         Output packet.
 * @param address     Device address (1..127).
 * @param cmd_data    Command + data bytes (plaintext, will be XOR'd).
 * @param cmd_len     Length of cmd_data.
 * @param global_key  The global key.
 * @param message_key The per-message key.
 * @return            Total packet size in bytes, or -1 on error.
 */
int orion_packet_build_encrypted(orion_packet_t *pkt, uint8_t address,
                                 const uint8_t *cmd_data, size_t cmd_len,
                                 uint8_t global_key, uint8_t message_key);

/**
 * Serialize an orion_packet_t into a byte buffer for transmission.
 *
 * @param pkt    The packet to serialize.
 * @param buf    Output buffer (must be at least ORION_MAX_PACKET bytes).
 * @return       Number of bytes written to buf, or -1 on error.
 */
int orion_packet_serialize(const orion_packet_t *pkt, uint8_t *buf);

/**
 * Parse a received byte buffer into an orion_packet_t.
 *
 * @param buf    Input buffer.
 * @param len    Length of input buffer.
 * @param pkt    Output packet.
 * @return       0 on success, -1 on error (CRC mismatch, etc.).
 */
int orion_packet_parse(const uint8_t *buf, size_t len, orion_packet_t *pkt);

/**
 * Decrypt the payload of an encrypted packet in-place.
 *
 * @param pkt         The packet whose payload will be decrypted.
 * @param message_key The message key used for decryption.
 */
void orion_packet_decrypt_payload(orion_packet_t *pkt, uint8_t message_key);

/**
 * Send a packet and receive a response (synchronous transaction).
 *
 * @param ctx     Protocol context.
 * @param request The request packet to send.
 * @param response Output buffer for the response packet.
 * @return        0 on success, -1 on error.
 */
int orion_transact(orion_ctx_t *ctx, const orion_packet_t *request,
                   orion_packet_t *response);

#endif /* ORION_PROTOCOL_H */
