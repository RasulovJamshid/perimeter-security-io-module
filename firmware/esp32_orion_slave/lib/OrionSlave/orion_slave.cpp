#include "orion_slave.h"

/* Inter-byte timeout: if no new byte arrives within this many ms,
   we consider the current packet complete (or garbage). */
#define INTER_BYTE_TIMEOUT_MS  5

/* Delay before responding — RS-485 bus turnaround time.
   С2000М expects a response within ~50ms but not instantly. */
#define RESPONSE_DELAY_US  1500

OrionSlave::OrionSlave(HardwareSerial &serial, uint8_t address, int de_pin)
    : _serial(serial)
    , _address(address)
    , _de_pin(de_pin)
    , _device_type(ORION_DEV_SIGNAL_20P)
    , _fw_major(2)
    , _fw_minor(0)
    , _global_key(0x00)
    , _message_key(0x00)
    , _zone_count(4)
    , _event_head(0)
    , _event_tail(0)
    , _event_count(0)
    , _rx_pos(0)
    , _last_rx_time(0)
    , _packets_rx(0)
    , _packets_tx(0)
    , _cb_arm(nullptr)
    , _cb_disarm(nullptr)
    , _cb_reset(nullptr)
    , _cb_raw(nullptr)
{
    memset(_zones, 0, sizeof(_zones));
    memset(_events, 0, sizeof(_events));
    memset(_rx_buf, 0, sizeof(_rx_buf));
}

void OrionSlave::begin(unsigned long baud)
{
    _serial.begin(baud, SERIAL_8N1);

    if (_de_pin >= 0) {
        pinMode(_de_pin, OUTPUT);
        enableReceive();
    }

    /* Initialize all configured zones as disarmed */
    for (uint8_t i = 0; i < _zone_count && i < ORION_MAX_ZONES; i++) {
        _zones[i].status = ORION_STATUS_DISARMED;
        _zones[i].enabled = 1;
    }

    Serial.printf("[OrionSlave] Started: addr=%d, type=0x%02X, zones=%d\n",
                  _address, _device_type, _zone_count);
}

void OrionSlave::setDeviceType(uint8_t type) { _device_type = type; }
void OrionSlave::setFirmwareVersion(uint8_t major, uint8_t minor) { _fw_major = major; _fw_minor = minor; }
void OrionSlave::setGlobalKey(uint8_t key) { _global_key = key; _message_key = key; }
void OrionSlave::setZoneCount(uint8_t count) {
    _zone_count = (count > ORION_MAX_ZONES) ? ORION_MAX_ZONES : count;
    for (uint8_t i = 0; i < _zone_count; i++) {
        if (!_zones[i].enabled) {
            _zones[i].status = ORION_STATUS_DISARMED;
            _zones[i].enabled = 1;
        }
    }
}

void OrionSlave::setZoneStatus(uint8_t zone, uint8_t status)
{
    if (zone > 0 && zone <= ORION_MAX_ZONES) {
        _zones[zone - 1].status = status;
    }
}

uint8_t OrionSlave::getZoneStatus(uint8_t zone) const
{
    if (zone > 0 && zone <= ORION_MAX_ZONES) {
        return _zones[zone - 1].status;
    }
    return 0;
}

bool OrionSlave::pushEvent(uint8_t event_code, uint8_t zone, uint8_t partition)
{
    if (_event_count >= ORION_MAX_EVENTS) {
        return false; /* Queue full */
    }
    _events[_event_head].event_code = event_code;
    _events[_event_head].zone = zone;
    _events[_event_head].partition = partition;
    _event_head = (_event_head + 1) % ORION_MAX_EVENTS;
    _event_count++;
    return true;
}

bool OrionSlave::popEvent(orion_event_t *evt)
{
    if (_event_count == 0) return false;
    *evt = _events[_event_tail];
    _event_tail = (_event_tail + 1) % ORION_MAX_EVENTS;
    _event_count--;
    return true;
}

/* ─── RS-485 direction control ─────────────────────────────────────── */

void OrionSlave::enableTransmit()
{
    if (_de_pin >= 0) {
        digitalWrite(_de_pin, HIGH);
        delayMicroseconds(50); /* Settle time */
    }
}

void OrionSlave::enableReceive()
{
    if (_de_pin >= 0) {
        digitalWrite(_de_pin, LOW);
    }
}

void OrionSlave::sendResponse(const uint8_t *data, size_t len)
{
    delayMicroseconds(RESPONSE_DELAY_US);
    enableTransmit();
    _serial.write(data, len);
    _serial.flush(); /* Wait for TX complete */
    enableReceive();
    _packets_tx++;
}

/* ─── Main polling loop ────────────────────────────────────────────── */

void OrionSlave::poll()
{
    uint32_t now = millis();

    /* Check for inter-byte timeout — if we have partial data and
       no new bytes arrive, reset the buffer */
    if (_rx_pos > 0 && (now - _last_rx_time) > INTER_BYTE_TIMEOUT_MS) {
        /* We have a complete packet candidate (or garbage) */
        if (_rx_pos >= 3) {
            /* Minimum valid packet: [addr][len][crc] = 3 bytes */
            uint8_t addr = _rx_buf[0];
            uint8_t pkt_len = _rx_buf[1]; /* bytes after address, incl CRC */
            size_t expected_total = 2 + pkt_len; /* addr + len_byte + payload + crc */

            bool for_us = (addr == _address) ||
                          (addr == (_address + ORION_ENCRYPT_OFFSET));

            if (_cb_raw) {
                _cb_raw(_rx_buf, _rx_pos, for_us);
            }

            if (for_us && _rx_pos >= expected_total) {
                /* Verify CRC */
                size_t crc_data_len = 2 + (pkt_len - 1); /* everything except CRC byte */
                uint8_t expected_crc = orion_crc8(_rx_buf, crc_data_len);
                uint8_t received_crc = _rx_buf[crc_data_len];

                if (expected_crc == received_crc) {
                    _packets_rx++;
                    processPacket(_rx_buf, expected_total);
                }
            }
        }
        _rx_pos = 0;
    }

    /* Read available bytes */
    while (_serial.available()) {
        if (_rx_pos >= ORION_MAX_PACKET) {
            _rx_pos = 0; /* Buffer overflow — reset */
        }
        _rx_buf[_rx_pos++] = _serial.read();
        _last_rx_time = millis();
    }
}

/* ─── Packet processing ────────────────────────────────────────────── */

void OrionSlave::processPacket(const uint8_t *data, size_t len)
{
    uint8_t addr = data[0];
    uint8_t pkt_len = data[1];
    size_t payload_len = pkt_len - 1; /* Exclude CRC byte */
    const uint8_t *payload = &data[2];
    bool encrypted = (addr >= ORION_ENCRYPT_OFFSET);

    if (encrypted) {
        /* Encrypted packet: payload[0] = global_key ^ message_key
         * Remaining bytes are XOR'd with message_key */
        if (payload_len < 2) return;

        /* Derive message_key from the packet */
        uint8_t key_xor = payload[0];
        /* If global_key is known: message_key = key_xor ^ global_key */
        uint8_t msg_key = key_xor ^ _global_key;

        /* Decrypt command bytes */
        uint8_t decrypted[ORION_MAX_PAYLOAD];
        decrypted[0] = key_xor;
        for (size_t i = 1; i < payload_len; i++) {
            decrypted[i] = payload[i] ^ msg_key;
        }

        /* The actual command is in decrypted[1] */
        uint8_t cmd = decrypted[1];
        _message_key = msg_key;

        switch (cmd) {
            case ORION_CMD_READ_STATUS:
                handleReadStatus(decrypted, payload_len);
                break;
            case ORION_CMD_ARM_ZONE:
                handleArm(decrypted, payload_len);
                break;
            case ORION_CMD_DISARM_ZONE:
                handleDisarm(decrypted, payload_len);
                break;
            case ORION_CMD_RESET_ALARM:
                handleResetAlarm(decrypted, payload_len);
                break;
            case ORION_CMD_READ_EVENT:
                handleReadEvent();
                break;
            case ORION_CMD_CONFIRM_EVENT:
                handleConfirmEvent();
                break;
            default:
                Serial.printf("[OrionSlave] Unknown encrypted cmd: 0x%02X\n", cmd);
                break;
        }
    } else {
        /* Unencrypted packet */
        if (payload_len < 1) return;

        /* For unencrypted packets, command layout varies.
         * Common: payload[0] = key_xor, payload[1] = command
         * Or for simple commands: payload[0] = command directly
         */
        uint8_t cmd;

        if (payload_len >= 2 && payload[1] == ORION_CMD_SET_GLOBAL_KEY) {
            cmd = payload[1];
            handleSetGlobalKey(payload, payload_len);
        } else {
            cmd = payload[0];
            switch (cmd) {
                case ORION_CMD_PING:
                    handlePing();
                    break;
                case ORION_CMD_READ_DEVICE_TYPE:
                    handleReadDeviceType();
                    break;
                case ORION_CMD_READ_VERSION:
                    handleReadVersion();
                    break;
                default:
                    /* Try byte[1] as command (after key_xor) */
                    if (payload_len >= 2) {
                        cmd = payload[1];
                        switch (cmd) {
                            case ORION_CMD_READ_DEVICE_TYPE:
                                handleReadDeviceType();
                                break;
                            case ORION_CMD_READ_VERSION:
                                handleReadVersion();
                                break;
                            default:
                                Serial.printf("[OrionSlave] Unknown plain cmd: 0x%02X\n", cmd);
                                break;
                        }
                    }
                    break;
            }
        }
    }
}

/* ─── Command handlers ─────────────────────────────────────────────── */

void OrionSlave::handlePing()
{
    /* Respond with a simple acknowledgement: [addr][02][ack_byte][crc] */
    uint8_t resp_payload[1] = { 0x06 }; /* ACK byte */
    uint8_t buf[ORION_MAX_PACKET];
    size_t len = buildPlainResponse(buf, resp_payload, 1);
    sendResponse(buf, len);
}

void OrionSlave::handleReadDeviceType()
{
    /* Response: device type byte */
    uint8_t resp_payload[1] = { _device_type };
    uint8_t buf[ORION_MAX_PACKET];
    size_t len = buildPlainResponse(buf, resp_payload, 1);
    sendResponse(buf, len);
    Serial.printf("[OrionSlave] Reported device type: 0x%02X\n", _device_type);
}

void OrionSlave::handleReadVersion()
{
    /* Response: major, minor firmware version */
    uint8_t resp_payload[2] = { _fw_major, _fw_minor };
    uint8_t buf[ORION_MAX_PACKET];
    size_t len = buildPlainResponse(buf, resp_payload, 2);
    sendResponse(buf, len);
}

void OrionSlave::handleSetGlobalKey(const uint8_t *payload, size_t len)
{
    /* payload[0] = old key_xor, payload[1] = 0x11, payload[2] = new_key, payload[3] = new_key */
    if (len >= 4) {
        uint8_t new_key = payload[2];
        _global_key = new_key;
        _message_key = new_key;
        Serial.printf("[OrionSlave] Global key set to 0x%02X\n", new_key);

        /* ACK */
        uint8_t resp_payload[1] = { 0x06 };
        uint8_t buf[ORION_MAX_PACKET];
        size_t rlen = buildPlainResponse(buf, resp_payload, 1);
        sendResponse(buf, rlen);
    }
}

void OrionSlave::handleReadStatus(const uint8_t *payload, size_t len)
{
    /* Build encrypted status response.
     * Response payload (before encryption):
     *   [0x88] [0x02] [msg_key] [zone_count] [zone_idx]
     *   [STATUS_1] [STATUS_2] [extra]
     */
    uint8_t zone_idx = 0;
    if (len >= 3) {
        zone_idx = payload[2]; /* Zone number requested */
    }

    uint8_t status1 = ORION_STATUS_POWER_OK;
    uint8_t status2 = ORION_STATUS_CONNECTION_OK;

    if (zone_idx > 0 && zone_idx <= _zone_count) {
        status1 = _zones[zone_idx - 1].status;
    } else if (_zone_count > 0) {
        /* Default: report zone 1 status */
        status1 = _zones[0].status;
    }

    uint8_t resp_plain[7];
    resp_plain[0] = 0x88;         /* Response marker */
    resp_plain[1] = 0x02;         /* Sub-command echo */
    resp_plain[2] = _zone_count;  /* Number of zones */
    resp_plain[3] = zone_idx;     /* Requested zone */
    resp_plain[4] = status1;      /* STATUS_1 */
    resp_plain[5] = status2;      /* STATUS_2 */
    resp_plain[6] = 0x00;         /* Extra byte */

    uint8_t buf[ORION_MAX_PACKET];
    size_t rlen = buildEncryptedResponse(buf, resp_plain, 7);
    sendResponse(buf, rlen);
}

void OrionSlave::handleArm(const uint8_t *payload, size_t len)
{
    uint8_t zone = (len >= 3) ? payload[2] : 1;
    bool accepted = true;

    if (_cb_arm) {
        accepted = _cb_arm(zone);
    }

    if (accepted && zone > 0 && zone <= _zone_count) {
        _zones[zone - 1].status = ORION_STATUS_ARMED;
        pushEvent(ORION_STATUS_ARMED, zone, 0);
        Serial.printf("[OrionSlave] Zone %d ARMED\n", zone);
    }

    /* ACK or NACK */
    uint8_t ack = accepted ? 0x06 : 0x15;
    uint8_t buf[ORION_MAX_PACKET];
    size_t rlen = buildEncryptedResponse(buf, &ack, 1);
    sendResponse(buf, rlen);
}

void OrionSlave::handleDisarm(const uint8_t *payload, size_t len)
{
    uint8_t zone = (len >= 3) ? payload[2] : 1;
    bool accepted = true;

    if (_cb_disarm) {
        accepted = _cb_disarm(zone);
    }

    if (accepted && zone > 0 && zone <= _zone_count) {
        _zones[zone - 1].status = ORION_STATUS_DISARMED;
        pushEvent(ORION_STATUS_DISARMED, zone, 0);
        Serial.printf("[OrionSlave] Zone %d DISARMED\n", zone);
    }

    uint8_t ack = accepted ? 0x06 : 0x15;
    uint8_t buf[ORION_MAX_PACKET];
    size_t rlen = buildEncryptedResponse(buf, &ack, 1);
    sendResponse(buf, rlen);
}

void OrionSlave::handleResetAlarm(const uint8_t *payload, size_t len)
{
    uint8_t zone = (len >= 3) ? payload[2] : 1;

    if (zone > 0 && zone <= _zone_count) {
        _zones[zone - 1].status = ORION_STATUS_DISARMED;
        Serial.printf("[OrionSlave] Zone %d alarm RESET\n", zone);
    }

    if (_cb_reset) {
        _cb_reset(zone);
    }

    uint8_t ack = 0x06;
    uint8_t buf[ORION_MAX_PACKET];
    size_t rlen = buildEncryptedResponse(buf, &ack, 1);
    sendResponse(buf, rlen);
}

void OrionSlave::handleReadEvent()
{
    orion_event_t evt;
    if (popEvent(&evt)) {
        uint8_t resp[3];
        resp[0] = evt.event_code;
        resp[1] = evt.zone;
        resp[2] = evt.partition;

        uint8_t buf[ORION_MAX_PACKET];
        size_t rlen = buildEncryptedResponse(buf, resp, 3);
        sendResponse(buf, rlen);
    } else {
        /* No events — respond with empty/no-event marker */
        uint8_t resp[1] = { 0x00 };
        uint8_t buf[ORION_MAX_PACKET];
        size_t rlen = buildEncryptedResponse(buf, resp, 1);
        sendResponse(buf, rlen);
    }
}

void OrionSlave::handleConfirmEvent()
{
    /* Just ACK — the event was already popped in handleReadEvent */
    uint8_t ack = 0x06;
    uint8_t buf[ORION_MAX_PACKET];
    size_t rlen = buildPlainResponse(buf, &ack, 1);
    sendResponse(buf, rlen);
}

/* ─── Packet building ──────────────────────────────────────────────── */

size_t OrionSlave::buildPlainResponse(uint8_t *buf, const uint8_t *payload, size_t payload_len)
{
    /* [address] [length] [payload...] [crc]
     * length = payload_len + 1 (for CRC byte) */
    buf[0] = _address;
    buf[1] = (uint8_t)(payload_len + 1);
    memcpy(&buf[2], payload, payload_len);

    uint8_t crc = orion_crc8(buf, 2 + payload_len);
    buf[2 + payload_len] = crc;

    return 2 + payload_len + 1;
}

size_t OrionSlave::buildEncryptedResponse(uint8_t *buf, const uint8_t *payload, size_t payload_len)
{
    /* Encrypted response:
     * [address+0x80] [length] [key_xor] [encrypted_payload...] [msg_key_pad] [crc]
     */
    size_t enc_len = 1 + payload_len + 1; /* key_xor + encrypted + 1 padding */
    uint8_t enc_payload[ORION_MAX_PAYLOAD];

    enc_payload[0] = _global_key ^ _message_key; /* key_xor */

    for (size_t i = 0; i < payload_len; i++) {
        enc_payload[1 + i] = payload[i] ^ _message_key;
    }
    enc_payload[1 + payload_len] = _message_key; /* padding */

    buf[0] = _address + ORION_ENCRYPT_OFFSET;
    buf[1] = (uint8_t)(enc_len + 1); /* +1 for CRC */
    memcpy(&buf[2], enc_payload, enc_len);

    uint8_t crc = orion_crc8(buf, 2 + enc_len);
    buf[2 + enc_len] = crc;

    return 2 + enc_len + 1;
}
