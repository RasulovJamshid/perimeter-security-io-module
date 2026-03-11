#ifndef ORION_SLAVE_H
#define ORION_SLAVE_H

#include <Arduino.h>
#include <stdint.h>
#include "orion_crc.h"

/* Orion protocol constants */
#define ORION_MAX_PACKET        256
#define ORION_MAX_PAYLOAD       252
#define ORION_ENCRYPT_OFFSET    0x80
#define ORION_MAX_ADDRESS       127

/* Command codes sent by С2000М master */
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

/* Device type codes (emulate a known Bolid device) */
#define ORION_DEV_SIGNAL_20P    0x34  /* Signal-20P alarm panel */
#define ORION_DEV_S2000_4       0x0A  /* С2000-4 relay module */
#define ORION_DEV_S2000_KDL     0x16  /* С2000-КДЛ loop controller */
#define ORION_DEV_CUSTOM        0xFE  /* Custom/unknown device type */

/* Status codes */
#define ORION_STATUS_ARMED          0x04
#define ORION_STATUS_DISARMED       0x06
#define ORION_STATUS_ALARM          0x09
#define ORION_STATUS_TAMPER         0x95
#define ORION_STATUS_POWER_OK       0xC7
#define ORION_STATUS_CONNECTION_OK  0xD1

/* Maximum zones per emulated device */
#define ORION_MAX_ZONES  20

/* Maximum queued events */
#define ORION_MAX_EVENTS 32

/**
 * Event structure for the event queue.
 */
typedef struct {
    uint8_t  event_code;
    uint8_t  zone;
    uint8_t  partition;
} orion_event_t;

/**
 * Zone state.
 */
typedef struct {
    uint8_t status;     /* Current status code (armed, disarmed, alarm, etc.) */
    uint8_t enabled;    /* 1 = zone exists, 0 = zone not configured */
} orion_zone_t;

/**
 * Callback: called when С2000М sends an arm command.
 * Return true to accept, false to reject.
 */
typedef bool (*orion_arm_callback_t)(uint8_t zone);

/**
 * Callback: called when С2000М sends a disarm command.
 */
typedef bool (*orion_disarm_callback_t)(uint8_t zone);

/**
 * Callback: called when С2000М sends a reset alarm command.
 */
typedef void (*orion_reset_callback_t)(uint8_t zone);

/**
 * Callback: called on any raw incoming packet (for debugging/forwarding).
 */
typedef void (*orion_raw_callback_t)(const uint8_t *data, size_t len, bool is_for_us);

/**
 * OrionSlave — emulates a Bolid Orion device on the RS-485 bus.
 *
 * The С2000М master polls devices round-robin. When our address is polled,
 * we respond with the appropriate data. We never transmit unsolicited.
 */
class OrionSlave {
public:
    OrionSlave(HardwareSerial &serial, uint8_t address, int de_pin);

    /* Configuration */
    void begin(unsigned long baud = 9600);
    void setDeviceType(uint8_t type);
    void setFirmwareVersion(uint8_t major, uint8_t minor);
    void setGlobalKey(uint8_t key);
    void setZoneCount(uint8_t count);

    /* Zone management */
    void setZoneStatus(uint8_t zone, uint8_t status);
    uint8_t getZoneStatus(uint8_t zone) const;

    /* Event queue — events are reported to С2000М when it polls us */
    bool pushEvent(uint8_t event_code, uint8_t zone, uint8_t partition);

    /* Callbacks */
    void onArm(orion_arm_callback_t cb)     { _cb_arm = cb; }
    void onDisarm(orion_disarm_callback_t cb) { _cb_disarm = cb; }
    void onReset(orion_reset_callback_t cb)  { _cb_reset = cb; }
    void onRawPacket(orion_raw_callback_t cb) { _cb_raw = cb; }

    /* Main loop — call this from loop(), handles bus communication */
    void poll();

    /* Stats */
    uint32_t getPacketsReceived() const { return _packets_rx; }
    uint32_t getPacketsAnswered() const { return _packets_tx; }

private:
    HardwareSerial &_serial;
    uint8_t  _address;
    int      _de_pin;       /* MAX485 DE/RE pin (HIGH=transmit, LOW=receive) */
    uint8_t  _device_type;
    uint8_t  _fw_major;
    uint8_t  _fw_minor;
    uint8_t  _global_key;
    uint8_t  _message_key;
    uint8_t  _zone_count;

    /* Zone states */
    orion_zone_t _zones[ORION_MAX_ZONES];

    /* Circular event queue */
    orion_event_t _events[ORION_MAX_EVENTS];
    uint8_t _event_head;
    uint8_t _event_tail;
    uint8_t _event_count;

    /* Receive buffer */
    uint8_t  _rx_buf[ORION_MAX_PACKET];
    size_t   _rx_pos;
    uint32_t _last_rx_time;

    /* Stats */
    uint32_t _packets_rx;
    uint32_t _packets_tx;

    /* Callbacks */
    orion_arm_callback_t    _cb_arm;
    orion_disarm_callback_t _cb_disarm;
    orion_reset_callback_t  _cb_reset;
    orion_raw_callback_t    _cb_raw;

    /* Internal methods */
    void enableTransmit();
    void enableReceive();
    void sendResponse(const uint8_t *data, size_t len);
    void processPacket(const uint8_t *data, size_t len);

    /* Command handlers */
    void handlePing();
    void handleReadDeviceType();
    void handleReadVersion();
    void handleSetGlobalKey(const uint8_t *payload, size_t len);
    void handleReadStatus(const uint8_t *payload, size_t len);
    void handleArm(const uint8_t *payload, size_t len);
    void handleDisarm(const uint8_t *payload, size_t len);
    void handleResetAlarm(const uint8_t *payload, size_t len);
    void handleReadEvent();
    void handleConfirmEvent();

    /* Packet building helpers */
    size_t buildPlainResponse(uint8_t *buf, const uint8_t *payload, size_t payload_len);
    size_t buildEncryptedResponse(uint8_t *buf, const uint8_t *payload, size_t payload_len);

    /* Event queue helpers */
    bool popEvent(orion_event_t *evt);
};

#endif /* ORION_SLAVE_H */
