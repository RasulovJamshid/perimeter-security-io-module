#ifndef ORION_BUS_MONITOR_H
#define ORION_BUS_MONITOR_H

#include <Arduino.h>
#include <stdint.h>
#include "orion_crc.h"

/* ─── Constants ────────────────────────────────────────────────────── */

#define ORION_MAX_DEVICES       127
#define ORION_MAX_PACKET        256
#define ORION_ENCRYPT_OFFSET    0x80
#define ORION_MAX_ZONES_PER_DEV 20
#define ORION_EVENT_LOG_SIZE    64

/* ─── Known command codes (from С2000М master) ─────────────────────── */

#define ORION_CMD_PING              0x01
#define ORION_CMD_READ_DEVICE_TYPE  0x06
#define ORION_CMD_READ_VERSION      0x07
#define ORION_CMD_SET_GLOBAL_KEY    0x11
#define ORION_CMD_READ_EVENT        0x20
#define ORION_CMD_CONFIRM_EVENT     0x21
#define ORION_CMD_READ_STATUS       0x57
#define ORION_CMD_DISARM_ZONE       0x63
#define ORION_CMD_ARM_ZONE          0x64
#define ORION_CMD_RESET_ALARM       0x65

/* ─── Known device types ───────────────────────────────────────────── */

#define ORION_DEV_SIGNAL_20P    0x34
#define ORION_DEV_SIGNAL_20M    0x35
#define ORION_DEV_S2000_4       0x0A
#define ORION_DEV_S2000_KDL     0x16
#define ORION_DEV_S2000_2       0x09
#define ORION_DEV_S2000_BI      0x1A
#define ORION_DEV_S2000M        0x04

/* ─── Status codes ─────────────────────────────────────────────────── */

#define ORION_ST_ARMED          0x04
#define ORION_ST_DISARMED       0x06
#define ORION_ST_ALARM          0x09
#define ORION_ST_FIRE_ALARM     0x0A
#define ORION_ST_ENTRY_DELAY    0x0B
#define ORION_ST_EXIT_DELAY     0x0C
#define ORION_ST_TAMPER         0x95
#define ORION_ST_TAMPER_REST    0x96
#define ORION_ST_POWER_OK       0xC7
#define ORION_ST_POWER_FAIL     0xC8
#define ORION_ST_BATTERY_LOW    0xC9
#define ORION_ST_BATTERY_OK     0xCA
#define ORION_ST_CONN_LOST      0xD0
#define ORION_ST_CONN_OK        0xD1

/* ─── Data structures ──────────────────────────────────────────────── */

/**
 * Per-zone state tracked from bus traffic.
 */
typedef struct {
    uint8_t  status;        /* Last known status code */
    uint32_t last_update;   /* millis() of last status change */
} orion_zone_state_t;

/**
 * Per-device state tracked from bus traffic.
 */
typedef struct {
    bool     online;            /* Device responded to last poll */
    uint8_t  device_type;       /* Device type code (0x34 = Signal-20P, etc.) */
    uint8_t  fw_major;          /* Firmware major version */
    uint8_t  fw_minor;          /* Firmware minor version */
    uint8_t  zone_count;        /* Number of known zones */
    uint32_t last_seen;         /* millis() of last response from this device */
    uint32_t poll_count;        /* Number of polls observed */
    uint32_t response_count;    /* Number of responses observed */
    orion_zone_state_t zones[ORION_MAX_ZONES_PER_DEV];
} orion_device_state_t;

/**
 * Event logged from bus traffic.
 */
typedef struct {
    uint8_t  device_addr;   /* Source device address */
    uint8_t  event_code;    /* Event/status code */
    uint8_t  zone;          /* Zone number (0 = device-level) */
    uint8_t  extra;         /* Extra data byte */
    uint32_t timestamp;     /* millis() when captured */
} orion_bus_event_t;

/**
 * Decoded packet for external forwarding.
 */
typedef struct {
    uint8_t  addr;          /* Device address (1..127) */
    bool     encrypted;     /* Was the packet encrypted? */
    bool     is_request;    /* true = from master, false = from slave */
    uint8_t  command;       /* Command code (if decoded) */
    uint8_t  payload[ORION_MAX_PACKET];
    size_t   payload_len;
    uint32_t timestamp;
} orion_decoded_packet_t;

/* ─── Callbacks ────────────────────────────────────────────────────── */

/**
 * Called when a device status changes (zone armed, alarm, etc.).
 */
typedef void (*orion_status_change_cb_t)(uint8_t device_addr, uint8_t zone,
                                         uint8_t old_status, uint8_t new_status);

/**
 * Called when a device comes online or goes offline.
 */
typedef void (*orion_device_online_cb_t)(uint8_t device_addr, bool online);

/**
 * Called for every decoded packet (for raw forwarding).
 */
typedef void (*orion_packet_cb_t)(const orion_decoded_packet_t *pkt);

/**
 * Called when an event is captured from the bus.
 */
typedef void (*orion_event_cb_t)(const orion_bus_event_t *evt);

/* ─── OrionBusMonitor class ────────────────────────────────────────── */

/**
 * Passively monitors the Orion RS-485 bus, decodes all traffic between
 * С2000М and all devices, and maintains a state table.
 *
 * Does NOT transmit on the bus — completely passive and safe.
 */
class OrionBusMonitor {
public:
    OrionBusMonitor(HardwareSerial &serial, int de_pin = -1);

    /* Setup */
    void begin(unsigned long baud = 9600);
    void setGlobalKey(uint8_t key);

    /* Main loop — call from loop() */
    void poll();

    /* Device state queries */
    const orion_device_state_t* getDevice(uint8_t addr) const;
    bool isDeviceOnline(uint8_t addr) const;
    uint8_t getDeviceType(uint8_t addr) const;
    uint8_t getZoneStatus(uint8_t addr, uint8_t zone) const;
    uint8_t getOnlineDeviceCount() const;

    /* Get list of online device addresses */
    uint8_t getOnlineDevices(uint8_t *addrs, uint8_t max_count) const;

    /* Event log */
    uint8_t getEventCount() const { return _event_count; }
    const orion_bus_event_t* getEvent(uint8_t index) const;

    /* Callbacks */
    void onStatusChange(orion_status_change_cb_t cb) { _cb_status = cb; }
    void onDeviceOnline(orion_device_online_cb_t cb) { _cb_online = cb; }
    void onPacket(orion_packet_cb_t cb)              { _cb_packet = cb; }
    void onEvent(orion_event_cb_t cb)                { _cb_event = cb; }

    /* Stats */
    uint32_t getTotalPackets() const { return _total_packets; }
    uint32_t getCrcErrors() const    { return _crc_errors; }

    /* Generate JSON status for all devices */
    size_t toJSON(char *buf, size_t buf_size) const;

    /* Generate JSON status for a single device */
    size_t deviceToJSON(uint8_t addr, char *buf, size_t buf_size) const;

    /* Device offline timeout (ms) — if no response within this time, mark offline */
    void setOfflineTimeout(uint32_t ms) { _offline_timeout = ms; }

    /* Status code to human-readable string */
    static const char* statusStr(uint8_t status);
    static const char* deviceTypeStr(uint8_t type);

private:
    HardwareSerial &_serial;
    int      _de_pin;
    uint8_t  _global_key;

    /* Device state table */
    orion_device_state_t _devices[ORION_MAX_DEVICES];

    /* Event circular log */
    orion_bus_event_t _events[ORION_EVENT_LOG_SIZE];
    uint8_t  _event_head;
    uint8_t  _event_count;

    /* Receive buffer + packet framing */
    uint8_t  _rx_buf[ORION_MAX_PACKET];
    size_t   _rx_pos;
    uint32_t _last_rx_time;  /* Microseconds (from micros()) - sub-ms precision */

    /* Packet direction tracking:
     * On RS-485 we see both master requests and slave responses.
     * Master always sends first; the response follows within ~50ms.
     * We track the last request address to know the context of responses.
     */
    uint8_t  _last_request_addr;
    uint8_t  _last_request_cmd;
    uint32_t _last_request_time;

    /* Stats */
    uint32_t _total_packets;
    uint32_t _crc_errors;
    uint32_t _offline_timeout;

    /* Callbacks */
    orion_status_change_cb_t _cb_status;
    orion_device_online_cb_t _cb_online;
    orion_packet_cb_t        _cb_packet;
    orion_event_cb_t         _cb_event;

    /* Internal */
    void processPacket(const uint8_t *data, size_t len);
    void decodeRequest(uint8_t addr, const uint8_t *payload, size_t len, bool encrypted);
    void decodeResponse(uint8_t addr, const uint8_t *payload, size_t len, bool encrypted);
    void decryptPayload(const uint8_t *in, uint8_t *out, size_t len, uint8_t *msg_key_out);
    void updateDeviceStatus(uint8_t addr, uint8_t zone, uint8_t status);
    void logEvent(uint8_t addr, uint8_t code, uint8_t zone, uint8_t extra);
    void checkOfflineDevices();
};

#endif /* ORION_BUS_MONITOR_H */
