#ifndef ORION_MASTER_H
#define ORION_MASTER_H

#include <Arduino.h>
#include <stdint.h>
#include "orion_crc.h"
#include "orion_bus_monitor.h"

/* ─── Operating modes ─────────────────────────────────────────────── */

typedef enum {
    ORION_MODE_MONITOR = 0,   /* Passive: DE pin LOW, only listen (default) */
    ORION_MODE_MASTER  = 1    /* Active: polls devices, sends commands */
} orion_operating_mode_t;

/* ─── Master command queue ────────────────────────────────────────── */

#define ORION_CMD_QUEUE_SIZE  16
#define ORION_RESPONSE_TIMEOUT_MS  200
#define ORION_MASTER_POLL_INTERVAL_MS  100
#define ORION_MASTER_DETECT_TIMEOUT_MS 30000  /* No master traffic for 30s = master absent */

typedef struct {
    uint8_t  address;
    uint8_t  cmd_data[16];
    size_t   cmd_len;
    bool     encrypted;
} orion_queued_cmd_t;

/* ─── Callback for mode changes ───────────────────────────────────── */

typedef void (*orion_mode_change_cb_t)(orion_operating_mode_t old_mode,
                                       orion_operating_mode_t new_mode,
                                       const char *reason);

/* ─── OrionMaster class ───────────────────────────────────────────── */

/**
 * Extends the bus monitor with master/controller capability.
 *
 * TWO MODES in one project:
 *   MONITOR (default) — passive, never transmits, safe to use alongside C2000M
 *   MASTER            — acts as C2000M replacement, polls and commands devices
 *
 * SAFETY:
 *   - Defaults to MONITOR mode on boot
 *   - Auto-detects if real C2000M master is present on bus
 *   - If master traffic detected while in MASTER mode, immediately
 *     falls back to MONITOR to prevent bus collisions
 *   - MASTER mode requires explicit activation (serial cmd, WiFi API, or button)
 */
class OrionMaster {
public:
    OrionMaster(HardwareSerial &serial, int de_pin);

    /* Setup — call after OrionBusMonitor::begin() */
    void begin();

    /* Set encryption key (same as bus monitor) */
    void setGlobalKey(uint8_t key)  { _global_key = key; }
    void setMessageKey(uint8_t key) { _message_key = key; }

    /* ─── Mode control ─────────────────────────────────── */

    orion_operating_mode_t getMode() const { return _mode; }

    /**
     * Switch to MASTER mode.
     * Returns false if master PC is detected on bus (unsafe to switch).
     */
    bool switchToMaster();

    /** Switch back to MONITOR mode (always succeeds). */
    void switchToMonitor();

    /** Call from loop() — handles polling, command queue, master detection */
    void poll();

    /** Notify that a request packet was seen on the bus (call from bus monitor).
     *  Used for master-presence detection. */
    void notifyBusRequest(uint8_t addr, uint8_t cmd);

    /* ─── Master detection ─────────────────────────────── */

    /** Is a real C2000M master currently active on the bus? */
    bool isMasterDetected() const { return _master_detected; }

    /** Milliseconds since last master traffic */
    uint32_t msSinceLastMasterTraffic() const;

    /** Set timeout for master detection (default 30s) */
    void setMasterDetectTimeout(uint32_t ms) { _master_detect_timeout = ms; }

    /* ─── Command sending (only works in MASTER mode) ──── */

    /** Ping a device (unencrypted) */
    int cmdPing(uint8_t address);

    /** Read device type and firmware version (unencrypted) */
    int cmdReadDeviceInfo(uint8_t address);

    /** Read device status (encrypted) */
    int cmdReadStatus(uint8_t address);

    /** Read event from device (encrypted) */
    int cmdReadEvent(uint8_t address);

    /** Confirm/acknowledge event (encrypted) */
    int cmdConfirmEvent(uint8_t address);

    /** Arm a zone (encrypted) */
    int cmdArmZone(uint8_t address, uint8_t zone);

    /** Disarm a zone (encrypted) */
    int cmdDisarmZone(uint8_t address, uint8_t zone);

    /** Reset alarm on a zone (encrypted) */
    int cmdResetAlarm(uint8_t address, uint8_t zone);

    /* ─── Auto-polling (MASTER mode) ───────────────────── */

    /** Enable/disable automatic round-robin polling of devices */
    void setAutoPolling(bool enable) { _auto_polling = enable; }
    bool isAutoPolling() const { return _auto_polling; }

    /** Set polling interval (ms between polls) */
    void setPollInterval(uint32_t ms) { _poll_interval = ms; }

    /** Set which addresses to poll (1-127). By default polls 1-127. */
    void setPollRange(uint8_t min_addr, uint8_t max_addr);

    /* ─── Callbacks ────────────────────────────────────── */

    void onModeChange(orion_mode_change_cb_t cb) { _cb_mode = cb; }

    /* ─── Stats ────────────────────────────────────────── */

    uint32_t getTxPackets() const { return _tx_packets; }
    uint32_t getTxErrors() const  { return _tx_errors; }
    uint32_t getResponseTimeouts() const { return _response_timeouts; }

private:
    HardwareSerial &_serial;
    int      _de_pin;
    uint8_t  _global_key;
    uint8_t  _message_key;

    /* Operating mode */
    orion_operating_mode_t _mode;

    /* Master detection */
    bool     _master_detected;
    uint32_t _last_master_traffic;
    uint32_t _master_detect_timeout;

    /* Auto-polling state */
    bool     _auto_polling;
    uint8_t  _poll_addr_min;
    uint8_t  _poll_addr_max;
    uint8_t  _poll_current_addr;
    uint32_t _last_poll_time;
    uint32_t _poll_interval;
    uint8_t  _poll_phase;  /* 0=ping, 1=read_type, 2=read_status, 3=read_event */

    /* Stats */
    uint32_t _tx_packets;
    uint32_t _tx_errors;
    uint32_t _response_timeouts;

    /* Callbacks */
    orion_mode_change_cb_t _cb_mode;

    /* ─── Internal packet building & TX ────────────────── */

    /** Build unencrypted packet: [addr][len][payload][crc] */
    int buildPacket(uint8_t *buf, uint8_t address,
                    const uint8_t *cmd_data, size_t cmd_len);

    /** Build encrypted packet: [addr+0x80][len][key_xor][enc_payload][padding][crc] */
    int buildEncryptedPacket(uint8_t *buf, uint8_t address,
                             const uint8_t *cmd_data, size_t cmd_len);

    /** Transmit a packet on RS-485 (toggles DE pin) */
    int transmitPacket(const uint8_t *buf, size_t len);

    /** Send command and wait for response */
    int sendAndReceive(uint8_t address, const uint8_t *cmd_data, size_t cmd_len,
                       bool encrypted, uint8_t *resp_buf, size_t *resp_len);

    /** Update master detection state */
    void updateMasterDetection();

    /** Execute next auto-poll step */
    void doAutoPoll();

    /** Set DE pin state */
    void setDE(bool high);
};

#endif /* ORION_MASTER_H */
