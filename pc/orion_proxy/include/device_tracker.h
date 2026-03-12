#ifndef DEVICE_TRACKER_H
#define DEVICE_TRACKER_H

#include <stdint.h>
#include <time.h>
#include "orion_protocol.h"
#include "orion_commands.h"

#define TRACKER_MAX_DEVICES     128   /* 0..127 */
#define TRACKER_MAX_EVENTS      256   /* Ring buffer size */
#define TRACKER_OFFLINE_SEC     30    /* Seconds before device marked offline */

typedef struct {
    uint8_t  address;
    int      online;               /* 1 = online, 0 = offline */
    uint8_t  device_type;          /* Orion device type code */
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  last_status1;
    uint8_t  last_status2;
    time_t   last_seen;            /* Timestamp of last packet from/to this device */
    uint32_t packet_count;         /* Total packets involving this device */
} tracked_device_t;

typedef struct {
    uint32_t id;                   /* Sequential event ID */
    time_t   timestamp;
    uint8_t  device_addr;
    uint8_t  event_code;
    uint8_t  zone;
    uint8_t  raw_payload[16];
    size_t   raw_len;
} tracked_event_t;

typedef struct {
    tracked_device_t devices[TRACKER_MAX_DEVICES];
    tracked_event_t  events[TRACKER_MAX_EVENTS];
    uint32_t         event_write_pos;     /* Next write position in ring buffer */
    uint32_t         event_count;         /* Total events ever recorded */
    uint32_t         total_packets;
    uint32_t         total_crc_errors;
    time_t           start_time;

#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    /* pthread_mutex_t for POSIX — simplified: use volatile for single-writer */
    volatile int     lock_flag;
#endif
} device_tracker_t;

/**
 * Initialize the device tracker.
 */
void tracker_init(device_tracker_t *t);

/**
 * Destroy the device tracker (free resources).
 */
void tracker_destroy(device_tracker_t *t);

/**
 * Called when sniffer receives a valid packet.
 * Updates device state and records events.
 */
void tracker_on_packet(device_tracker_t *t, const orion_packet_t *pkt,
                       const uint8_t *raw, size_t raw_len);

/**
 * Called when sniffer receives invalid data (CRC error).
 */
void tracker_on_crc_error(device_tracker_t *t);

/**
 * Check for devices that haven't been seen recently and mark offline.
 * Should be called periodically (e.g., every 5 seconds).
 */
void tracker_check_timeouts(device_tracker_t *t);

/**
 * Get the number of online devices.
 */
int tracker_online_count(device_tracker_t *t);

/**
 * Format device list as text lines into buffer.
 * Returns number of bytes written.
 */
int tracker_format_devices(device_tracker_t *t, char *buf, size_t buf_size);

/**
 * Format system status as text into buffer.
 * Returns number of bytes written.
 */
int tracker_format_status(device_tracker_t *t, char *buf, size_t buf_size);

/**
 * Format recent events as text lines into buffer.
 * @param max_events Maximum number of recent events to include.
 * Returns number of bytes written.
 */
int tracker_format_events(device_tracker_t *t, char *buf, size_t buf_size,
                          int max_events);

/**
 * Format single device info as text into buffer.
 * Returns number of bytes written, or 0 if device not found.
 */
int tracker_format_device(device_tracker_t *t, uint8_t address,
                          char *buf, size_t buf_size);

#endif /* DEVICE_TRACKER_H */
