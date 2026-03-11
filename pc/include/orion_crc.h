#ifndef ORION_CRC_H
#define ORION_CRC_H

#include <stdint.h>
#include <stddef.h>

/**
 * Calculate CRC-8-Dallas checksum used by the Bolid Orion protocol.
 *
 * @param data   Pointer to the message bytes.
 * @param length Number of bytes.
 * @return       CRC-8 checksum value.
 */
uint8_t orion_crc8(const uint8_t *data, size_t length);

#endif /* ORION_CRC_H */
