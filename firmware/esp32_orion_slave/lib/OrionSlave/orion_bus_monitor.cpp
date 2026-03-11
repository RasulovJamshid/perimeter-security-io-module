#include "orion_bus_monitor.h"

/* ═══════════════════════════════════════════════════════════════════════
 *  UART FRAMING & TIMING
 * ═══════════════════════════════════════════════════════════════════════
 * 
 * Hybrid Framing Approach (Length-based + Timeout-based):
 * 
 * 1. LENGTH-BASED (Primary):
 *    - Orion protocol: [addr][len][payload...][crc]
 *    - Once we receive addr+len bytes, we know exact packet size
 *    - Process immediately when buffer reaches expected_total bytes
 *    - Result: Zero latency, no waiting for timeout
 * 
 * 2. TIMEOUT-BASED (Fallback):
 *    - If 5ms silence detected on bus, process whatever is in buffer
 *    - Uses micros() instead of millis() for sub-millisecond precision
 *    - Immune to FreeRTOS scheduler jitter (mqtt.publish(), WiFi tasks)
 *    - At 9600 baud: 1 byte = ~1.04ms, so 5ms is safe frame boundary
 * 
 * Benefits:
 * - Instant packet processing (no 5ms wait for valid packets)
 * - Robust against timing jitter from WiFi/MQTT tasks
 * - Handles malformed packets gracefully via timeout
 * - Dramatically improved response time in MASTER mode
 * 
 * ═══════════════════════════════════════════════════════════════════════ */

#define INTER_BYTE_TIMEOUT_US   5000  /* 5ms in microseconds - more precise than millis() */
#define REQUEST_RESPONSE_GAP_MS 50
#define DEFAULT_OFFLINE_MS      30000

OrionBusMonitor::OrionBusMonitor(HardwareSerial &serial, int de_pin)
    : _serial(serial)
    , _de_pin(de_pin)
    , _global_key(0x00)
    , _event_head(0)
    , _event_count(0)
    , _rx_pos(0)
    , _last_rx_time(0)
    , _last_request_addr(0)
    , _last_request_cmd(0)
    , _last_request_time(0)
    , _total_packets(0)
    , _crc_errors(0)
    , _offline_timeout(DEFAULT_OFFLINE_MS)
    , _cb_status(nullptr)
    , _cb_online(nullptr)
    , _cb_packet(nullptr)
    , _cb_event(nullptr)
{
    memset(_devices, 0, sizeof(_devices));
    memset(_events, 0, sizeof(_events));
    memset(_rx_buf, 0, sizeof(_rx_buf));
}

void OrionBusMonitor::begin(unsigned long baud)
{
    _serial.begin(baud, SERIAL_8N1);

    /* If DE pin specified, keep it LOW (receive mode only) */
    if (_de_pin >= 0) {
        pinMode(_de_pin, OUTPUT);
        digitalWrite(_de_pin, LOW);
    }

    Serial.println("[BusMonitor] Started — passive listen mode");
}

void OrionBusMonitor::setGlobalKey(uint8_t key)
{
    _global_key = key;
}

/* ─── Main polling loop ────────────────────────────────────────────── */

void OrionBusMonitor::poll()
{
    uint32_t now_us = micros();  /* Use micros() for sub-millisecond precision */
    uint32_t now_ms = millis();  /* Still need millis() for offline check */

    /* Timeout-based framing: detect end of packet by silence on bus
     * Using micros() instead of millis() to avoid FreeRTOS scheduler jitter */
    if (_rx_pos > 0 && (now_us - _last_rx_time) > INTER_BYTE_TIMEOUT_US) {
        if (_rx_pos >= 3) {
            processPacket(_rx_buf, _rx_pos);
        }
        _rx_pos = 0;
    }

    /* Read available bytes from bus */
    while (_serial.available()) {
        if (_rx_pos >= ORION_MAX_PACKET) {
            _rx_pos = 0;  /* Buffer overflow protection */
        }
        _rx_buf[_rx_pos++] = _serial.read();
        _last_rx_time = micros();  /* Update with microsecond precision */

        /* Hybrid framing: Length-based packet detection
         * Orion protocol: [addr][len][payload...][crc]
         * If we have addr+len bytes, and buffer matches expected length,
         * process immediately without waiting for timeout.
         * This dramatically improves response time and eliminates timing jitter issues.
         */
        if (_rx_pos >= 2) {
            uint8_t expected_total = 2 + _rx_buf[1];  /* addr + len + payload + crc */
            if (_rx_pos == expected_total && expected_total >= 3) {
                processPacket(_rx_buf, _rx_pos);
                _rx_pos = 0;  /* Reset immediately - no timeout wait needed */
            }
        }
    }

    /* Periodic: check for devices gone offline */
    static uint32_t last_offline_check = 0;
    if (now_ms - last_offline_check > 5000) {
        last_offline_check = now_ms;
        checkOfflineDevices();
    }
}

/* ─── Packet processing ────────────────────────────────────────────── */

void OrionBusMonitor::processPacket(const uint8_t *data, size_t len)
{
    if (len < 3) return;

    uint8_t raw_addr = data[0];
    uint8_t pkt_len = data[1];
    bool encrypted = (raw_addr >= ORION_ENCRYPT_OFFSET);
    uint8_t addr = encrypted ? (raw_addr - ORION_ENCRYPT_OFFSET) : raw_addr;

    if (addr == 0 || addr > ORION_MAX_DEVICES) return;

    size_t expected_total = 2 + pkt_len;
    if (len < expected_total) return;

    /* CRC check */
    size_t crc_data_len = 2 + (pkt_len - 1);
    uint8_t expected_crc = orion_crc8(data, crc_data_len);
    uint8_t received_crc = data[crc_data_len];

    if (expected_crc != received_crc) {
        _crc_errors++;
        return;
    }

    _total_packets++;

    size_t payload_len = pkt_len - 1;
    const uint8_t *payload = &data[2];

    /* Determine if this is a request (from master) or response (from slave).
     *
     * Heuristic: On the Orion bus, С2000М sends requests and devices respond.
     * The master polls devices round-robin. If we see a packet to an address
     * and recently saw a request to the same address, this is likely a response.
     *
     * Master requests come first; slave responses follow within ~50ms.
     */
    uint32_t now = millis();
    bool is_response = false;

    if (_last_request_addr == addr &&
        (now - _last_request_time) < REQUEST_RESPONSE_GAP_MS) {
        is_response = true;
    }

    /* Build decoded packet for callback */
    orion_decoded_packet_t decoded;
    decoded.addr = addr;
    decoded.encrypted = encrypted;
    decoded.is_request = !is_response;
    decoded.command = 0;
    decoded.payload_len = payload_len;
    decoded.timestamp = now;
    if (payload_len > 0 && payload_len <= sizeof(decoded.payload)) {
        memcpy(decoded.payload, payload, payload_len);
    }

    if (is_response) {
        decodeResponse(addr, payload, payload_len, encrypted);
    } else {
        decodeRequest(addr, payload, payload_len, encrypted);
        _last_request_addr = addr;
        _last_request_time = now;
    }

    /* Try to extract command code for the decoded packet */
    if (encrypted && payload_len >= 2) {
        uint8_t msg_key = payload[0] ^ _global_key;
        decoded.command = payload[1] ^ msg_key;
    } else if (!encrypted && payload_len >= 1) {
        decoded.command = payload[0];
    }

    if (_cb_packet) {
        _cb_packet(&decoded);
    }
}

/* ─── Decode master requests ───────────────────────────────────────── */

void OrionBusMonitor::decodeRequest(uint8_t addr, const uint8_t *payload,
                                     size_t len, bool encrypted)
{
    if (len < 1) return;

    uint8_t cmd = 0;

    if (encrypted && len >= 2) {
        uint8_t msg_key = payload[0] ^ _global_key;
        cmd = payload[1] ^ msg_key;
    } else {
        cmd = payload[0];
    }

    _last_request_cmd = cmd;

    /* Track that this device was polled */
    orion_device_state_t &dev = _devices[addr - 1];
    dev.poll_count++;
}

/* ─── Decode slave responses ───────────────────────────────────────── */

void OrionBusMonitor::decodeResponse(uint8_t addr, const uint8_t *payload,
                                      size_t len, bool encrypted)
{
    if (len < 1) return;

    orion_device_state_t &dev = _devices[addr - 1];
    uint32_t now = millis();

    /* Mark device as online */
    if (!dev.online) {
        dev.online = true;
        if (_cb_online) _cb_online(addr, true);
    }
    dev.last_seen = now;
    dev.response_count++;

    uint8_t cmd = _last_request_cmd;

    if (encrypted && len >= 2) {
        /* Decrypt response */
        uint8_t msg_key = payload[0] ^ _global_key;
        uint8_t decrypted[ORION_MAX_PACKET];
        size_t dec_len = len - 1;

        for (size_t i = 1; i < len; i++) {
            decrypted[i - 1] = payload[i] ^ msg_key;
        }

        /* Interpret based on what command was sent */
        switch (cmd) {
            case ORION_CMD_READ_STATUS:
                if (dec_len >= 3) {
                    /* Response typically: [response_marker] [sub_cmd] [zone_count]
                     * [zone_idx] [status1] [status2] ... */
                    uint8_t status_byte = decrypted[2];
                    uint8_t zone = (dec_len >= 2) ? decrypted[1] : 0;
                    updateDeviceStatus(addr, zone, status_byte);
                }
                break;

            case ORION_CMD_READ_EVENT:
                if (dec_len >= 2) {
                    uint8_t event_code = decrypted[0];
                    uint8_t zone = decrypted[1];
                    uint8_t extra = (dec_len >= 3) ? decrypted[2] : 0;
                    if (event_code != 0x00) {
                        logEvent(addr, event_code, zone, extra);
                        updateDeviceStatus(addr, zone, event_code);
                    }
                }
                break;

            case ORION_CMD_ARM_ZONE:
            case ORION_CMD_DISARM_ZONE:
            case ORION_CMD_RESET_ALARM:
                /* ACK/NACK response — the status change will be
                 * visible on the next status poll */
                break;

            default:
                break;
        }

    } else {
        /* Unencrypted response */
        switch (cmd) {
            case ORION_CMD_READ_DEVICE_TYPE:
                if (len >= 1) {
                    dev.device_type = payload[0];
                }
                break;

            case ORION_CMD_READ_VERSION:
                if (len >= 2) {
                    dev.fw_major = payload[0];
                    dev.fw_minor = payload[1];
                }
                break;

            case ORION_CMD_PING:
                /* Device responded to ping — already marked online above */
                break;

            default:
                break;
        }
    }
}

/* ─── Encryption helper ────────────────────────────────────────────── */

void OrionBusMonitor::decryptPayload(const uint8_t *in, uint8_t *out,
                                      size_t len, uint8_t *msg_key_out)
{
    if (len < 2) return;
    uint8_t msg_key = in[0] ^ _global_key;
    if (msg_key_out) *msg_key_out = msg_key;
    out[0] = in[0];
    for (size_t i = 1; i < len; i++) {
        out[i] = in[i] ^ msg_key;
    }
}

/* ─── State management ─────────────────────────────────────────────── */

void OrionBusMonitor::updateDeviceStatus(uint8_t addr, uint8_t zone,
                                          uint8_t status)
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) return;
    orion_device_state_t &dev = _devices[addr - 1];

    if (zone > 0 && zone <= ORION_MAX_ZONES_PER_DEV) {
        uint8_t old_status = dev.zones[zone - 1].status;
        if (old_status != status) {
            dev.zones[zone - 1].status = status;
            dev.zones[zone - 1].last_update = millis();

            if (zone > dev.zone_count) {
                dev.zone_count = zone;
            }

            if (_cb_status) {
                _cb_status(addr, zone, old_status, status);
            }
        }
    }
}

void OrionBusMonitor::logEvent(uint8_t addr, uint8_t code,
                                uint8_t zone, uint8_t extra)
{
    orion_bus_event_t &evt = _events[_event_head];
    evt.device_addr = addr;
    evt.event_code = code;
    evt.zone = zone;
    evt.extra = extra;
    evt.timestamp = millis();

    _event_head = (_event_head + 1) % ORION_EVENT_LOG_SIZE;
    if (_event_count < ORION_EVENT_LOG_SIZE) _event_count++;

    if (_cb_event) {
        _cb_event(&evt);
    }
}

void OrionBusMonitor::checkOfflineDevices()
{
    uint32_t now = millis();
    for (uint8_t i = 0; i < ORION_MAX_DEVICES; i++) {
        if (_devices[i].online &&
            _devices[i].last_seen > 0 &&
            (now - _devices[i].last_seen) > _offline_timeout) {
            _devices[i].online = false;
            if (_cb_online) _cb_online(i + 1, false);
        }
    }
}

/* ─── Query methods ────────────────────────────────────────────────── */

const orion_device_state_t* OrionBusMonitor::getDevice(uint8_t addr) const
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) return nullptr;
    return &_devices[addr - 1];
}

bool OrionBusMonitor::isDeviceOnline(uint8_t addr) const
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) return false;
    return _devices[addr - 1].online;
}

uint8_t OrionBusMonitor::getDeviceType(uint8_t addr) const
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) return 0;
    return _devices[addr - 1].device_type;
}

uint8_t OrionBusMonitor::getZoneStatus(uint8_t addr, uint8_t zone) const
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) return 0;
    if (zone == 0 || zone > ORION_MAX_ZONES_PER_DEV) return 0;
    return _devices[addr - 1].zones[zone - 1].status;
}

uint8_t OrionBusMonitor::getOnlineDeviceCount() const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < ORION_MAX_DEVICES; i++) {
        if (_devices[i].online) count++;
    }
    return count;
}

uint8_t OrionBusMonitor::getOnlineDevices(uint8_t *addrs, uint8_t max_count) const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < ORION_MAX_DEVICES && count < max_count; i++) {
        if (_devices[i].online) {
            addrs[count++] = i + 1;
        }
    }
    return count;
}

const orion_bus_event_t* OrionBusMonitor::getEvent(uint8_t index) const
{
    if (index >= _event_count) return nullptr;
    uint8_t pos;
    if (_event_count < ORION_EVENT_LOG_SIZE) {
        pos = index;
    } else {
        pos = (_event_head + index) % ORION_EVENT_LOG_SIZE;
    }
    return &_events[pos];
}

/* ─── JSON serialization ───────────────────────────────────────────── */

size_t OrionBusMonitor::toJSON(char *buf, size_t buf_size) const
{
    int pos = snprintf(buf, buf_size,
        "{\"online\":%d,\"total_pkts\":%lu,\"crc_errs\":%lu,\"devices\":[",
        getOnlineDeviceCount(),
        (unsigned long)_total_packets,
        (unsigned long)_crc_errors);

    bool first = true;
    for (uint8_t i = 0; i < ORION_MAX_DEVICES; i++) {
        if (!_devices[i].online && _devices[i].last_seen == 0) continue;

        if (!first) pos += snprintf(buf + pos, buf_size - pos, ",");
        first = false;

        pos += snprintf(buf + pos, buf_size - pos,
            "{\"addr\":%d,\"online\":%s,\"type\":%d,\"type_s\":\"%s\","
            "\"fw\":\"%d.%d\",\"zones\":%d,\"polls\":%lu,\"resps\":%lu",
            i + 1,
            _devices[i].online ? "true" : "false",
            _devices[i].device_type,
            deviceTypeStr(_devices[i].device_type),
            _devices[i].fw_major, _devices[i].fw_minor,
            _devices[i].zone_count,
            (unsigned long)_devices[i].poll_count,
            (unsigned long)_devices[i].response_count);

        /* Add zone statuses if any */
        if (_devices[i].zone_count > 0) {
            pos += snprintf(buf + pos, buf_size - pos, ",\"zst\":[");
            for (uint8_t z = 0; z < _devices[i].zone_count && z < ORION_MAX_ZONES_PER_DEV; z++) {
                if (z > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
                pos += snprintf(buf + pos, buf_size - pos,
                    "{\"z\":%d,\"s\":%d,\"s_str\":\"%s\"}",
                    z + 1,
                    _devices[i].zones[z].status,
                    statusStr(_devices[i].zones[z].status));
            }
            pos += snprintf(buf + pos, buf_size - pos, "]");
        }

        pos += snprintf(buf + pos, buf_size - pos, "}");

        if ((size_t)pos >= buf_size - 10) break;
    }

    pos += snprintf(buf + pos, buf_size - pos, "]}");
    return (size_t)pos;
}

size_t OrionBusMonitor::deviceToJSON(uint8_t addr, char *buf, size_t buf_size) const
{
    if (addr == 0 || addr > ORION_MAX_DEVICES) {
        return snprintf(buf, buf_size, "{\"error\":\"invalid address\"}");
    }

    const orion_device_state_t &dev = _devices[addr - 1];

    int pos = snprintf(buf, buf_size,
        "{\"addr\":%d,\"online\":%s,\"type\":%d,\"type_s\":\"%s\","
        "\"fw\":\"%d.%d\",\"zones\":%d,\"zst\":[",
        addr,
        dev.online ? "true" : "false",
        dev.device_type,
        deviceTypeStr(dev.device_type),
        dev.fw_major, dev.fw_minor,
        dev.zone_count);

    for (uint8_t z = 0; z < dev.zone_count && z < ORION_MAX_ZONES_PER_DEV; z++) {
        if (z > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        pos += snprintf(buf + pos, buf_size - pos,
            "{\"z\":%d,\"s\":%d,\"s_str\":\"%s\"}",
            z + 1, dev.zones[z].status, statusStr(dev.zones[z].status));
    }

    pos += snprintf(buf + pos, buf_size - pos, "]}");
    return (size_t)pos;
}

/* ─── String helpers ───────────────────────────────────────────────── */

const char* OrionBusMonitor::statusStr(uint8_t status)
{
    switch (status) {
        case 0x00:              return "unknown";
        case ORION_ST_ARMED:    return "armed";
        case ORION_ST_DISARMED: return "disarmed";
        case ORION_ST_ALARM:    return "alarm";
        case ORION_ST_FIRE_ALARM: return "fire_alarm";
        case ORION_ST_ENTRY_DELAY: return "entry_delay";
        case ORION_ST_EXIT_DELAY:  return "exit_delay";
        case ORION_ST_TAMPER:      return "tamper";
        case ORION_ST_TAMPER_REST: return "tamper_restored";
        case ORION_ST_POWER_OK:    return "power_ok";
        case ORION_ST_POWER_FAIL:  return "power_fail";
        case ORION_ST_BATTERY_LOW: return "battery_low";
        case ORION_ST_BATTERY_OK:  return "battery_ok";
        case ORION_ST_CONN_LOST:   return "conn_lost";
        case ORION_ST_CONN_OK:     return "conn_ok";
        default: return "other";
    }
}

const char* OrionBusMonitor::deviceTypeStr(uint8_t type)
{
    switch (type) {
        case 0x00:                  return "unknown";
        case ORION_DEV_S2000M:      return "S2000M";
        case ORION_DEV_SIGNAL_20P:  return "Signal-20P";
        case ORION_DEV_SIGNAL_20M:  return "Signal-20M";
        case ORION_DEV_S2000_4:     return "S2000-4";
        case ORION_DEV_S2000_KDL:   return "S2000-KDL";
        case ORION_DEV_S2000_2:     return "S2000-2";
        case ORION_DEV_S2000_BI:    return "S2000-BI";
        default: return "other";
    }
}
