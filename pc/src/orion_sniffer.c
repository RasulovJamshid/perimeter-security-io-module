#include "orion_sniffer.h"
#include "orion_crc.h"
#include "orion_commands.h"
#include <stdio.h>
#include <string.h>

/*
 * Passive RS-485 sniffer for the Bolid Orion bus.
 *
 * This module NEVER transmits on the bus. It only reads bytes and attempts
 * to reconstruct Orion packets by detecting the [address][length] framing.
 * It is safe to run alongside the existing С2000М master and desktop app.
 *
 * Framing strategy:
 *   - Read bytes continuously from the serial port.
 *   - When a byte arrives, treat it as a potential address byte.
 *   - Read the next byte as the length field.
 *   - Read 'length' more bytes (payload + CRC).
 *   - Validate with CRC-8. If valid, we have a packet.
 *   - If CRC fails, shift forward and try again.
 */

/* States for the packet framing state machine */
#define SNIFF_STATE_IDLE       0
#define SNIFF_STATE_GOT_ADDR   1
#define SNIFF_STATE_GOT_LEN    2
#define SNIFF_STATE_READING    3

void orion_hex_dump(const uint8_t *data, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        printf("%02X ", data[i]);
    }
}

static void default_print_packet(const orion_packet_t *pkt,
                                 const uint8_t *raw, size_t raw_len,
                                 int verbose, uint8_t known_key)
{
    uint8_t real_addr;
    const char *direction;
    orion_packet_t dec;
    size_t payload_len;

    real_addr = pkt->address;
    if (pkt->is_encrypted) {
        real_addr = pkt->address - 0x80;
    }

    /* Heuristic: requests from master tend to have addresses 1..127 or 129..255,
       responses echo the same address back. We can't fully distinguish direction
       without tracking state, but we print what we know. */
    direction = pkt->is_encrypted ? "ENC" : "RAW";

    printf("[%s] addr=%3d  len=%2d  ", direction, real_addr, pkt->length);

    if (verbose >= 1) {
        printf("hex: ");
        orion_hex_dump(raw, raw_len);
    }

    if (verbose >= 2 && pkt->is_encrypted && known_key != 0) {
        /* Attempt decryption for display */
        memcpy(&dec, pkt, sizeof(dec));
        orion_packet_decrypt_payload(&dec, known_key);

        payload_len = dec.length - 1;
        printf("\n       decrypted payload: ");
        if (payload_len > 0) {
            size_t i;
            for (i = 0; i < payload_len; ++i) {
                printf("%02X ", dec.payload[i]);
            }
        }

        /* Try to identify known status bytes */
        if (payload_len >= 7) {
            printf("\n       status1=0x%02X (%s)  status2=0x%02X (%s)",
                   dec.payload[5], orion_status_str(dec.payload[5]),
                   dec.payload[6], orion_status_str(dec.payload[6]));
        }
    }

    printf("\n");
    fflush(stdout);
}

int orion_sniffer_run(const orion_sniffer_config_t *config)
{
    uint8_t ring_buf[ORION_MAX_PACKET];
    uint8_t byte_buf[1];
    int state;
    size_t pkt_pos;
    size_t expected_remaining;
    int n;
    orion_packet_t pkt;

    if (!config || config->port == ORION_INVALID_PORT) {
        return -1;
    }

    printf("=== Orion Passive Sniffer ===\n");
    printf("Listening on bus (read-only, no transmissions)...\n");
    printf("Press Ctrl+C to stop.\n\n");
    fflush(stdout);

    state = SNIFF_STATE_IDLE;
    pkt_pos = 0;
    expected_remaining = 0;

    while (!(config->stop_flag && *config->stop_flag)) {
        n = orion_serial_read(config->port, byte_buf, 1);
        if (n < 1) {
            /* Timeout — no data on bus, just keep waiting */
            continue;
        }

        switch (state) {
        case SNIFF_STATE_IDLE:
            /* Potential address byte */
            ring_buf[0] = byte_buf[0];
            pkt_pos = 1;
            state = SNIFF_STATE_GOT_ADDR;
            break;

        case SNIFF_STATE_GOT_ADDR:
            /* Length byte */
            ring_buf[1] = byte_buf[0];
            pkt_pos = 2;

            if (byte_buf[0] == 0 || byte_buf[0] > ORION_MAX_PAYLOAD + 1) {
                /* Invalid length — restart */
                state = SNIFF_STATE_IDLE;
            } else {
                expected_remaining = byte_buf[0]; /* payload + CRC */
                state = SNIFF_STATE_READING;
            }
            break;

        case SNIFF_STATE_READING:
            ring_buf[pkt_pos++] = byte_buf[0];
            expected_remaining--;

            if (expected_remaining == 0) {
                /* We have a complete candidate packet — validate CRC */
                if (orion_packet_parse(ring_buf, pkt_pos, &pkt) == 0) {
                    /* Valid packet! */
                    if (config->on_packet) {
                        config->on_packet(&pkt, ring_buf, pkt_pos, config->user_data);
                    }
                    if (config->verbose > 0) {
                        default_print_packet(&pkt, ring_buf, pkt_pos,
                                             config->verbose, config->known_key);
                    }
                }
                /* Whether valid or not, go back to idle */
                state = SNIFF_STATE_IDLE;
                pkt_pos = 0;
            }

            /* Safety: prevent buffer overflow */
            if (pkt_pos >= ORION_MAX_PACKET) {
                state = SNIFF_STATE_IDLE;
                pkt_pos = 0;
            }
            break;

        default:
            state = SNIFF_STATE_IDLE;
            pkt_pos = 0;
            break;
        }
    }

    printf("\nSniffer stopped.\n");
    return 0;
}
