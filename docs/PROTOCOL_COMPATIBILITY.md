# Bolid Orion Protocol Compatibility Report

**Date:** 2024  
**Scope:** Verification of our Orion RS-485 protocol implementation against the real Bolid Orion protocol  
**Sources:** Habr article "Дешифрация протокола Орион Bolid", cxem.net forum, С2000-ПП documentation, S2000-PP English manual

---

## Executive Summary

Our implementation is **largely compatible** with the real Bolid Orion RS-485 protocol. One bug was found and fixed (status byte offset), and several minor gaps were identified for future improvement.

| Area | Status | Notes |
|------|--------|-------|
| CRC-8-Dallas | ✅ Match | Identical lookup table and algorithm |
| Packet format | ✅ Match | ADDRESS + LENGTH + PAYLOAD + CRC |
| Serial params | ✅ Match | 9600 baud, 8-N-1 |
| Encryption scheme | ✅ Match | XOR with message key, address +0x80 |
| Set Global Key (0x11) | ✅ Match | Unencrypted, key repeated |
| Read Status (0x57 0x02) | ✅ Match | Encrypted command |
| Status response parsing | ⚠️ Fixed | Off-by-one in status byte offsets |
| Arm/Disarm/Reset | ✅ Plausible | Not fully verified (no captures available) |
| Device type codes | ✅ Good coverage | 29 device types catalogued |
| Status codes | ✅ Good coverage | Matches known Bolid status values |
| Event system | ⚠️ Partial | Event parsing offsets need real-device validation |

---

## Detailed Findings

### 1. CRC-8-Dallas Checksum ✅

**Specification:** CRC-8 with polynomial `x^8 + x^5 + x^4 + 1` (0x31), initial value 0x00.

**Our table** (`pc/src/orion_crc.c`):
```
0x00,0x5E,0xBC,0xE2,0x61,0x3F,0xDD,0x83,...
```

**Reference table** (from Habr and cxem.net forum):
```
0,94,188,226,97,63,221,131,...
```

These are byte-for-byte identical (0x00=0, 0x5E=94, 0xBC=188, 0xE2=226, etc.).

**CRC scope:** Computed over `[ADDRESS, LENGTH, PAYLOAD]` — excludes the CRC byte itself.  
**Our implementation:** Matches exactly.

---

### 2. Packet Format ✅

**Real protocol:**
```
[ADDRESS:1] [LENGTH:1] [PAYLOAD:N] [CRC:1]
```

- **ADDRESS:** 1–127 for unencrypted, ADDRESS+128 (0x80) for encrypted
- **LENGTH:** Number of bytes following the address byte = payload_size + 1 (for CRC)
- **Max payload:** 252 bytes (our `ORION_MAX_PAYLOAD = 252`)
- **Max packet:** 256 bytes (our `ORION_MAX_PACKET = 256`)

**Our implementation** matches this layout exactly in `orion_packet_build()`, `orion_packet_serialize()`, and `orion_packet_parse()`.

---

### 3. Encryption ✅

**Real protocol encryption (from Habr):**

1. Address byte becomes `ADDR + 0x80`
2. Payload structure:
   - Byte 0: `GLOBAL_KEY XOR MESSAGE_KEY`
   - Bytes 1..N: Command data XOR'd with `MESSAGE_KEY`
   - Bytes N+1..N+3: `MESSAGE_KEY` repeated (padding)
3. CRC computed over encrypted data (including modified address)

**Our implementation** (`orion_packet_build_encrypted`):
```c
pkt->address = address + ORION_ENCRYPT_OFFSET;  // +0x80
pkt->payload[0] = global_key ^ message_key;
for (i = 0; i < cmd_len; i++)
    pkt->payload[1+i] = cmd_data[i] ^ message_key;
// 3 padding bytes of message_key
```

**Decryption** (`orion_packet_decrypt_payload`):
- Skips byte 0 (key_xor), XORs remaining payload with message_key.

Both match the real protocol.

---

### 4. Set Global Key Command ✅

**Real protocol (Habr example — device 3, new key 0xBA):**
```
TX: 03 06 00 11 BA BA 8D
```
- Unencrypted (address < 0x80)
- Payload: `[key_xor=0x00] [cmd=0x11] [new_key] [new_key]`

**Our implementation:**
```c
cmd_data[0] = ctx->global_key ^ ctx->message_key;
cmd_data[1] = ORION_CMD_SET_GLOBAL_KEY;  // 0x11
cmd_data[2] = new_key;
cmd_data[3] = new_key;
```

Exact match.

---

### 5. Read Status Command ✅

**Command codes:** `0x57` (read status), argument `0x02`.

**Our defines:**
```c
#define ORION_CMD_READ_STATUS     0x57
#define ORION_CMD_READ_STATUS_ARG 0x02
```

Match confirmed.

---

### 6. Status Response Parsing — BUG FOUND & FIXED ⚠️

**Issue:** Status bytes were being read from wrong payload offsets.

**Habr example** — response from device 3 (encrypted, after decryption):
```
Payload offsets: [0]=key_xor [1]=echo [2]=msg_key [3]=?? [4]=?? [5]=STATUS_1 [6]=STATUS_2 [7]=??
```

**Before fix (orion_commands.c):**
```c
status->status1 = response.payload[6];  // WRONG — off by one
status->status2 = response.payload[7];  // WRONG — off by one
```

**After fix:**
```c
status->status1 = response.payload[5];  // CORRECT
status->status2 = response.payload[6];  // CORRECT
```

The same fix was applied to `orion_sniffer.c` verbose output.

**Note:** `device_tracker.c` already had the correct offsets (5, 6).

---

### 7. Status Codes ✅

Our status code definitions match known Bolid values:

| Code | Our Define | Bolid Meaning |
|------|-----------|---------------|
| 0x80 | ORION_STATUS_ARMED | Armed |
| 0x81 | ORION_STATUS_ARMED_DELAY | Armed (delayed) |
| 0x83 | ORION_STATUS_DISARMED | Disarmed |
| 0x86 | ORION_STATUS_ALARM | Alarm |
| 0x95 | ORION_STATUS_TAMPER | Case tamper |
| 0xC7 | ORION_STATUS_POWER_RESTORED | Power restored |
| 0xC9 | ORION_STATUS_POWER_FAIL | Power failure |
| 0xD1 | ORION_STATUS_CONNECTION_OK | Connection OK |

These match the values documented in the Habr article and С2000-ПП reference.

---

### 8. Device Type Codes ✅

We catalog 29 Bolid device types. The codes match known Bolid type IDs:

| Code | Our Define | Device |
|------|-----------|--------|
| 0x01 | C2000M | System controller |
| 0x04 | Signal-20 | 20-zone panel |
| 0x06 | Signal-20M | 20-zone module |
| 0x0A | S2000-KDL | Loop controller |
| 0x10 | S2000-4 | 4-zone panel |
| 0x14 | S2000-PP | Protocol converter |
| ... | ... | (29 total) |

These are sourced from Bolid documentation and reverse-engineering.

---

### 9. Timing & Bus Arbitration ⚠️ Not Implemented

**Real protocol notes:**
- Master (С2000М) polls devices round-robin
- Response timeout: ~150ms typical
- Inter-packet gap: 3.5 character times (~4ms at 9600 baud)
- Our proxy acts as a **sniffer** (passive listener) or **master** (for transact)

**Our `orion_transact` timeout:** Configurable, default appears reasonable. No inter-packet gap enforcement for transmission was found, which could cause issues if sending commands rapidly.

**Recommendation:** Add configurable inter-packet delay for master mode.

---

### 10. Known Gaps / Future Work

1. **Event parsing offsets** (`orion_cmd_read_event`): The event_code, zone, and partition offsets (payload[1], [2], [3]) have not been verified against real device captures. These may need adjustment similar to the status fix.

2. **Orion Pro protocol:** The Habr article mentions "Orion" vs "Orion Pro" as two different encrypted protocol variants. Our implementation covers the standard "Orion" protocol. Orion Pro may use different encryption or additional features.

3. **Missing device types:** Newer Bolid devices (post-2020) may have type codes not in our table. The `Unknown` fallback handles this gracefully.

4. **Broadcast address (0x00):** The Bolid protocol uses address 0 for broadcast messages from the master. Our sniffer handles this, but command functions don't explicitly support broadcast.

5. **RS-485 direction control:** For half-duplex RS-485, proper TX enable/disable timing is important. Our serial abstraction handles this at the OS level, but custom USB-RS485 adapters may need explicit RTS/DTR control.

---

## Files Modified

| File | Change |
|------|--------|
| `pc/src/orion_commands.c` | Fixed status byte offsets from [6,7] to [5,6] |
| `pc/src/orion_sniffer.c` | Fixed status byte offsets in verbose display |

## Conclusion

The implementation is **production-ready** for the core protocol features (CRC, packet framing, encryption, key management, status reading). The status parsing bug has been fixed. Event parsing and some edge cases should be validated against real hardware when available.
