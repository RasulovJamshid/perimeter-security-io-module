#ifndef ORION_SERIAL_H
#define ORION_SERIAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE orion_port_t;
#define ORION_INVALID_PORT INVALID_HANDLE_VALUE
#else
typedef int orion_port_t;
#define ORION_INVALID_PORT (-1)
#endif

/**
 * Serial port configuration for Bolid Orion devices.
 * Default: 9600 baud, 8-N-1 (as per Orion protocol specification).
 */
typedef struct {
    const char *port_name;   /* e.g. "COM3" on Windows, "/dev/ttyUSB0" on Linux */
    uint32_t    baud_rate;   /* Default: 9600 */
    uint32_t    timeout_ms;  /* Read timeout in milliseconds */
} orion_serial_config_t;

/**
 * Open a serial port with Orion-compatible settings.
 *
 * @param config  Serial port configuration.
 * @return        Port handle, or ORION_INVALID_PORT on failure.
 */
orion_port_t orion_serial_open(const orion_serial_config_t *config);

/**
 * Close an open serial port.
 */
void orion_serial_close(orion_port_t port);

/**
 * Write bytes to the serial port.
 *
 * @return Number of bytes written, or -1 on error.
 */
int orion_serial_write(orion_port_t port, const uint8_t *data, size_t length);

/**
 * Read bytes from the serial port.
 *
 * @return Number of bytes read, or -1 on error.
 */
int orion_serial_read(orion_port_t port, uint8_t *buffer, size_t max_length);

/**
 * Flush both input and output buffers.
 */
void orion_serial_flush(orion_port_t port);

#endif /* ORION_SERIAL_H */
