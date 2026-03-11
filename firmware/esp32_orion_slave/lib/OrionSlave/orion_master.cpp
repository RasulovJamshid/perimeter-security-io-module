#include "orion_master.h"

OrionMaster::OrionMaster(HardwareSerial &serial, int de_pin)
    : _serial(serial)
    , _de_pin(de_pin)
    , _global_key(0x00)
    , _message_key(0x00)
    , _mode(ORION_MODE_MONITOR)
    , _master_detected(false)
    , _last_master_traffic(0)
    , _master_detect_timeout(ORION_MASTER_DETECT_TIMEOUT_MS)
    , _auto_polling(true)
    , _poll_addr_min(1)
    , _poll_addr_max(127)
    , _poll_current_addr(1)
    , _last_poll_time(0)
    , _poll_interval(ORION_MASTER_POLL_INTERVAL_MS)
    , _poll_phase(0)
    , _tx_packets(0)
    , _tx_errors(0)
    , _response_timeouts(0)
    , _cb_mode(nullptr)
{
}

void OrionMaster::begin()
{
    /* DE pin setup — starts LOW (receive mode) regardless of operating mode */
    if (_de_pin >= 0) {
        pinMode(_de_pin, OUTPUT);
        digitalWrite(_de_pin, LOW);
    }
    Serial.println("[Master] Initialized — default mode: MONITOR");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Mode control
 * ═══════════════════════════════════════════════════════════════════════ */

bool OrionMaster::switchToMaster()
{
    if (_mode == ORION_MODE_MASTER) return true;

    /* Safety check: don't switch if real master is active */
    if (_master_detected) {
        Serial.println("[Master] REFUSED — C2000M master detected on bus!");
        Serial.println("[Master] Disconnect C2000M first, then try again.");
        return false;
    }

    /* Additional safety: check time since last master traffic */
    if (_last_master_traffic > 0 &&
        (millis() - _last_master_traffic) < _master_detect_timeout) {
        Serial.printf("[Master] REFUSED — master traffic seen %lus ago (need %lus silence)\n",
            (millis() - _last_master_traffic) / 1000,
            _master_detect_timeout / 1000);
        return false;
    }

    orion_operating_mode_t old = _mode;
    _mode = ORION_MODE_MASTER;
    _poll_current_addr = _poll_addr_min;
    _poll_phase = 0;
    _last_poll_time = 0;

    Serial.println("[Master] *** SWITCHED TO MASTER MODE ***");
    Serial.println("[Master] ESP32 is now controlling the Orion bus");
    Serial.println("[Master] WARNING: Do NOT connect C2000M while in this mode!");

    if (_cb_mode) _cb_mode(old, _mode, "manual_switch");
    return true;
}

void OrionMaster::switchToMonitor()
{
    if (_mode == ORION_MODE_MONITOR) return;

    orion_operating_mode_t old = _mode;
    _mode = ORION_MODE_MONITOR;

    /* Ensure DE pin is LOW (receive only) */
    setDE(false);

    Serial.println("[Master] Switched to MONITOR mode (passive)");

    if (_cb_mode) _cb_mode(old, _mode, "manual_switch");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Main poll loop — call from loop()
 * ═══════════════════════════════════════════════════════════════════════ */

void OrionMaster::poll()
{
    updateMasterDetection();

    if (_mode == ORION_MODE_MASTER) {
        /* Safety: if master traffic detected, immediately fall back */
        if (_master_detected) {
            orion_operating_mode_t old = _mode;
            _mode = ORION_MODE_MONITOR;
            setDE(false);
            Serial.println("[Master] !!! C2000M DETECTED — emergency switch to MONITOR !!!");
            if (_cb_mode) _cb_mode(old, _mode, "master_detected");
            return;
        }

        /* Auto-polling */
        if (_auto_polling) {
            doAutoPoll();
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Master detection — watch for C2000M traffic on bus
 * ═══════════════════════════════════════════════════════════════════════ */

void OrionMaster::notifyBusRequest(uint8_t addr, uint8_t cmd)
{
    /* In MONITOR mode: this is how we detect the real master.
     * In MASTER mode: if we see requests we didn't send, another master is on bus! */
    if (_mode == ORION_MODE_MONITOR) {
        _last_master_traffic = millis();
        _master_detected = true;
    } else {
        /* We're in MASTER mode but saw a request we didn't send
         * — another master is on the bus! Emergency fallback. */
        _last_master_traffic = millis();
        _master_detected = true;
        /* poll() will handle the emergency switch */
    }
}

void OrionMaster::updateMasterDetection()
{
    if (_last_master_traffic == 0) {
        _master_detected = false;
        return;
    }

    uint32_t elapsed = millis() - _last_master_traffic;
    if (elapsed > _master_detect_timeout) {
        if (_master_detected) {
            _master_detected = false;
            Serial.printf("[Master] No master traffic for %lus — master appears offline\n",
                elapsed / 1000);
        }
    }
}

uint32_t OrionMaster::msSinceLastMasterTraffic() const
{
    if (_last_master_traffic == 0) return UINT32_MAX;
    return millis() - _last_master_traffic;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  DE pin control — HIGH = transmit, LOW = receive
 * ═══════════════════════════════════════════════════════════════════════ */

void OrionMaster::setDE(bool high)
{
    if (_de_pin >= 0) {
        digitalWrite(_de_pin, high ? HIGH : LOW);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Packet building
 * ═══════════════════════════════════════════════════════════════════════ */

int OrionMaster::buildPacket(uint8_t *buf, uint8_t address,
                              const uint8_t *cmd_data, size_t cmd_len)
{
    if (!buf || !cmd_data || address == 0 || address > ORION_MAX_DEVICES) return -1;
    if (cmd_len > 200) return -1;

    /* Packet: [address] [length] [payload...] [crc]
     * length = payload_len + 1 (for CRC byte) */
    buf[0] = address;
    buf[1] = (uint8_t)(cmd_len + 1);
    memcpy(&buf[2], cmd_data, cmd_len);

    /* CRC over: address + length + payload */
    uint8_t crc = orion_crc8(buf, 2 + cmd_len);
    buf[2 + cmd_len] = crc;

    return (int)(2 + cmd_len + 1);
}

int OrionMaster::buildEncryptedPacket(uint8_t *buf, uint8_t address,
                                       const uint8_t *cmd_data, size_t cmd_len)
{
    if (!buf || !cmd_data || address == 0 || address > ORION_MAX_DEVICES) return -1;
    if (cmd_len > 200) return -1;

    /* Encrypted packet:
     * [address + 0x80] [length] [key_xor] [enc_cmd...] [padding x3] [crc]
     *
     * key_xor = global_key ^ message_key
     * Each cmd byte XOR'd with message_key
     * 3 padding bytes = message_key
     */
    size_t enc_len = 1 + cmd_len + 3; /* key_xor + encrypted_cmd + 3 padding */

    buf[0] = address + ORION_ENCRYPT_OFFSET;
    buf[1] = (uint8_t)(enc_len + 1); /* +1 for CRC */

    /* Build encrypted payload */
    size_t idx = 2;
    buf[idx++] = _global_key ^ _message_key;

    for (size_t i = 0; i < cmd_len; i++) {
        buf[idx++] = cmd_data[i] ^ _message_key;
    }

    /* Padding */
    buf[idx++] = _message_key;
    buf[idx++] = _message_key;
    buf[idx++] = _message_key;

    /* CRC over everything except CRC itself */
    uint8_t crc = orion_crc8(buf, idx);
    buf[idx] = crc;

    return (int)(idx + 1);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Transmit on RS-485
 * ═══════════════════════════════════════════════════════════════════════ */

int OrionMaster::transmitPacket(const uint8_t *buf, size_t len)
{
    if (_mode != ORION_MODE_MASTER) {
        Serial.println("[Master] TX blocked — not in MASTER mode");
        return -1;
    }

    /* Toggle DE pin HIGH for transmit */
    setDE(true);
    delayMicroseconds(100); /* Let transceiver settle */

    size_t written = _serial.write(buf, len);
    _serial.flush(); /* Wait for all bytes to be sent */

    /* Toggle DE pin back to LOW (receive) */
    delayMicroseconds(100);
    setDE(false);

    if (written != len) {
        _tx_errors++;
        return -1;
    }

    _tx_packets++;
    return (int)written;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Send command and receive response
 * ═══════════════════════════════════════════════════════════════════════ */

int OrionMaster::sendAndReceive(uint8_t address, const uint8_t *cmd_data,
                                 size_t cmd_len, bool encrypted,
                                 uint8_t *resp_buf, size_t *resp_len)
{
    uint8_t tx_buf[ORION_MAX_PACKET];
    int tx_len;

    if (encrypted) {
        tx_len = buildEncryptedPacket(tx_buf, address, cmd_data, cmd_len);
    } else {
        tx_len = buildPacket(tx_buf, address, cmd_data, cmd_len);
    }

    if (tx_len < 0) return -1;

    /* Flush any pending RX data */
    while (_serial.available()) _serial.read();

    /* Transmit */
    int ret = transmitPacket(tx_buf, (size_t)tx_len);
    if (ret < 0) return -1;

    /* Wait for response */
    uint32_t start = millis();
    size_t rx_pos = 0;

    /* Read address + length (first 2 bytes) */
    while (rx_pos < 2 && (millis() - start) < ORION_RESPONSE_TIMEOUT_MS) {
        if (_serial.available()) {
            resp_buf[rx_pos++] = _serial.read();
        }
    }

    if (rx_pos < 2) {
        _response_timeouts++;
        return -1; /* No response */
    }

    /* Read remaining bytes (payload + CRC) based on length field */
    size_t remaining = resp_buf[1]; /* length = payload + CRC */
    while (rx_pos < 2 + remaining && (millis() - start) < ORION_RESPONSE_TIMEOUT_MS) {
        if (_serial.available()) {
            resp_buf[rx_pos++] = _serial.read();
        }
    }

    if (rx_pos < 2 + remaining) {
        _response_timeouts++;
        return -1; /* Incomplete response */
    }

    /* Verify CRC */
    size_t crc_data_len = 2 + (resp_buf[1] - 1);
    uint8_t expected_crc = orion_crc8(resp_buf, crc_data_len);
    if (expected_crc != resp_buf[crc_data_len]) {
        return -2; /* CRC error */
    }

    *resp_len = rx_pos;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Command implementations
 * ═══════════════════════════════════════════════════════════════════════ */

int OrionMaster::cmdPing(uint8_t address)
{
    uint8_t cmd[] = { ORION_CMD_PING };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), false, resp, &resp_len);
    if (ret == 0) {
        Serial.printf("[Master] Device %d: PING OK\n", address);
    }
    return ret;
}

int OrionMaster::cmdReadDeviceInfo(uint8_t address)
{
    uint8_t cmd[] = { ORION_CMD_READ_DEVICE_TYPE };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), false, resp, &resp_len);
    if (ret == 0 && resp_len >= 3) {
        size_t payload_len = resp[1] - 1;
        uint8_t dev_type = (payload_len >= 1) ? resp[2] : 0;
        Serial.printf("[Master] Device %d: type=0x%02X (%s)\n",
            address, dev_type,
            OrionBusMonitor::deviceTypeStr(dev_type));
    }
    return ret;
}

int OrionMaster::cmdReadStatus(uint8_t address)
{
    uint8_t cmd[] = { ORION_CMD_READ_STATUS, 0x02 };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
    if (ret == 0) {
        Serial.printf("[Master] Device %d: status response (%d bytes)\n",
            address, (int)resp_len);
    }
    return ret;
}

int OrionMaster::cmdReadEvent(uint8_t address)
{
    uint8_t cmd[] = { ORION_CMD_READ_EVENT };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    return sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
}

int OrionMaster::cmdConfirmEvent(uint8_t address)
{
    uint8_t cmd[] = { ORION_CMD_CONFIRM_EVENT };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    return sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
}

int OrionMaster::cmdArmZone(uint8_t address, uint8_t zone)
{
    uint8_t cmd[] = { ORION_CMD_ARM_ZONE, zone };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
    if (ret == 0) {
        Serial.printf("[Master] Armed zone %d on device %d\n", zone, address);
    }
    return ret;
}

int OrionMaster::cmdDisarmZone(uint8_t address, uint8_t zone)
{
    uint8_t cmd[] = { ORION_CMD_DISARM_ZONE, zone };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
    if (ret == 0) {
        Serial.printf("[Master] Disarmed zone %d on device %d\n", zone, address);
    }
    return ret;
}

int OrionMaster::cmdResetAlarm(uint8_t address, uint8_t zone)
{
    uint8_t cmd[] = { ORION_CMD_RESET_ALARM, zone };
    uint8_t resp[ORION_MAX_PACKET];
    size_t resp_len = 0;

    int ret = sendAndReceive(address, cmd, sizeof(cmd), true, resp, &resp_len);
    if (ret == 0) {
        Serial.printf("[Master] Reset alarm zone %d on device %d\n", zone, address);
    }
    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Auto-polling (MASTER mode)
 * ═══════════════════════════════════════════════════════════════════════ */

void OrionMaster::setPollRange(uint8_t min_addr, uint8_t max_addr)
{
    if (min_addr < 1) min_addr = 1;
    if (max_addr > ORION_MAX_DEVICES) max_addr = ORION_MAX_DEVICES;
    _poll_addr_min = min_addr;
    _poll_addr_max = max_addr;
    _poll_current_addr = min_addr;
}

void OrionMaster::doAutoPoll()
{
    uint32_t now = millis();
    if ((now - _last_poll_time) < _poll_interval) return;
    _last_poll_time = now;

    /* Round-robin polling:
     * Phase 0: Ping device
     * Phase 1: Read device type (first time only)
     * Phase 2: Read status
     * Phase 3: Read events
     *
     * Mimics C2000M behavior: poll each device in sequence.
     */
    switch (_poll_phase) {
        case 0: /* Ping */
            cmdPing(_poll_current_addr);
            _poll_phase = 1;
            break;

        case 1: /* Read device type (if not known yet) */
            cmdReadDeviceInfo(_poll_current_addr);
            _poll_phase = 2;
            break;

        case 2: /* Read status */
            cmdReadStatus(_poll_current_addr);
            _poll_phase = 3;
            break;

        case 3: /* Read events */
            cmdReadEvent(_poll_current_addr);

            /* Move to next device */
            _poll_phase = 0;
            _poll_current_addr++;
            if (_poll_current_addr > _poll_addr_max) {
                _poll_current_addr = _poll_addr_min;
            }
            break;
    }
}
