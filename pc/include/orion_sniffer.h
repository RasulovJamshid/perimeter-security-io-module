#ifndef ORION_SNIFFER_H
#define ORION_SNIFFER_H

#include <stdint.h>
#include "orion_serial.h"
#include "orion_protocol.h"

/**
 * Callback invoked when the sniffer captures a valid Orion packet.
 *
 * @param pkt         The parsed packet.
 * @param raw         Raw bytes as received.
 * @param raw_len     Length of raw bytes.
 * @param user_data   User-provided context pointer.
 */
typedef void (*orion_sniffer_callback_t)(const orion_packet_t *pkt,
                                         const uint8_t *raw, size_t raw_len,
                                         void *user_data);

/**
 * Sniffer configuration.
 */
typedef struct {
    orion_port_t             port;
    uint8_t                  known_key;      /* Message key for decrypting observed traffic (0 = no decrypt) */
    orion_sniffer_callback_t on_packet;      /* Called for each captured packet */
    void                    *user_data;      /* Passed to callback */
    int                      verbose;        /* 1 = print hex dumps, 2 = print decoded info */
    volatile int            *stop_flag;      /* Set to non-zero to stop the sniffer loop */
} orion_sniffer_config_t;

/**
 * Run the passive sniffer — listen-only mode on the RS-485 bus.
 *
 * This function does NOT transmit anything. It only reads bytes from the bus
 * and attempts to frame/decode Orion packets. Safe to use alongside the
 * existing С2000М master and desktop application.
 *
 * Blocks until *stop_flag is set to non-zero or an error occurs.
 *
 * @param config  Sniffer configuration.
 * @return        0 on clean stop, -1 on error.
 */
int orion_sniffer_run(const orion_sniffer_config_t *config);

/**
 * Print a hex dump of raw bytes to stdout.
 */
void orion_hex_dump(const uint8_t *data, size_t length);

#endif /* ORION_SNIFFER_H */
