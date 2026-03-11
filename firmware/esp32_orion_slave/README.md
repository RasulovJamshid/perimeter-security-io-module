# ESP32-C3 Orion RS-485 Gateway / Bridge

**Firmware Version: 1.0.0**

ESP32-C3 firmware with **dual-mode operation** for the Bolid Orion RS-485 bus:

- **MONITOR mode** (default) — passively listens to all bus traffic, safe alongside С2000М
- **MASTER mode** — replaces С2000М, polls devices and sends commands (arm/disarm/reset)

**Safety first** — starts in MONITOR mode. Automatically detects if С2000М is on the bus and prevents MASTER mode activation while it's present. If С2000М appears while in MASTER mode, immediately falls back to MONITOR.

**WiFi Access Point on-demand** — WiFi OFF by default for maximum performance. Press debug button → ESP32 creates its own WiFi network → connect and access HTTP configuration interface. Serial cable remains the primary output.

**Production-ready features**: Watchdog timer, heartbeat monitoring, LED indicators, physical mode switch, WiFi AP configuration interface, raw debug mode, NVS config storage.

## What It Does

### MONITOR Mode (default — safe alongside С2000М)

```
    Orion RS-485 Bus
    ═══╤═══════╤═══════╤═══════╤═══════╤═══
       │       │       │       │       │
    С2000М  Sig-20P  С2000-4  С2000-2  ESP32-C3 (passive listener)
    master  addr=1   addr=2   addr=3     │
                                         └──► Serial cable (UART0) ──► External System
```

### MASTER Mode (replaces С2000М — use only when PC master is disconnected)

```
    Orion RS-485 Bus
    ═══╤═══════╤═══════╤═══════╤═══
       │       │       │       │
    Sig-20P  С2000-4  С2000-2  ESP32-C3 (acting as master)
    addr=1   addr=2   addr=3     │
                                 ├──► Polls all devices (round-robin)
                                 ├──► Sends commands (arm/disarm/reset)
                                 ├──► Reads events from devices
                                 └──► Forwards everything via serial cable
```

- **Reads ALL bus traffic** — every request from С2000М, every response from every device
- **Tracks device states** — online/offline, device type, firmware version, zone statuses
- **Detects events** — alarms, arm/disarm, tamper, power failures
- **Auto-detects С2000М** — knows when the real master is present or absent
- **Serial cable output (UART0)** — direct wired connection to another system

## Bill of Materials (~$9-10)

| Component | Purpose | Price |
|-----------|---------|-------|
| ESP32-C3 Super Mini | Microcontroller (WiFi, BLE, USB-C) | ~$1.50 |
| MAX3485 module | RS-485 transceiver | ~$0.80 |
| MP2315 module | 12V→3.3V buck converter | ~$1.50 |
| LED green 3mm | Status indicator | ~$0.10 |
| LED red 3mm | Debug/mode indicator | ~$0.10 |
| LED yellow 3mm | Power indicator | ~$0.10 |
| Toggle switch SPDT | Mode switch (MONITOR/MASTER) | ~$0.40 |
| Push button | Debug button | ~$0.30 |
| Transistors, resistors | LED drivers, pull-ups | ~$0.50 |
| Wires, connectors | Wiring | ~$0.50 |

See `docs/BOM_ESP32C3_optimized.md` for complete component list.

## Pin Assignments (ESP32-C3 Super Mini)

| ESP32-C3 Pin | Function | Connects To |
|-----------|----------|-------------|
| **RS-485 Bus (UART1)** |
| GPIO7 (TX1) | RS-485 TX | MAX3485 DI |
| GPIO6 (RX1) | RS-485 RX | MAX3485 RO |
| GPIO2 | RS-485 DE/RE | MAX3485 DE+RE |
| **External Serial (UART0)** |
| GPIO21 (TX0) | External system TX | External RX |
| GPIO20 (RX0) | External system RX | External TX |
| **LEDs** |
| GPIO3 | Status LED (green) | Via transistor |
| GPIO4 | Debug LED (red) | Via transistor |
| **Buttons & Switches** |
| GPIO5 | Debug button | Push button (pull-up) |
| GPIO8 | Mode switch | SPDT toggle (pull-up) |
| **Power** |
| USB-C | Debug serial + power | PC (for programming/monitoring) |
| 12V input | Main power | Via MP2315 → 3.3V |

## Performance

This is a **serial-only firmware** — no WiFi, MQTT, HTTP, or OTA overhead.

| Metric | Value |
|--------|-------|
| **Loop time** | ~0.3-0.5ms |
| **CPU usage** | ~5-10% |
| **Max devices** | 127 (full bus) |
| **Flash usage** | ~350-400 KB |
| **RAM usage** | ~30-40 KB |

The firmware uses **hybrid UART framing** (length-based + timeout-based with `micros()`) for robust packet detection immune to timing jitter.

## WiFi Access Point (On-Demand Configuration)

WiFi is **OFF by default** for maximum performance. When you need to configure the device or view status:

### How to Enable WiFi AP

1. **Press the debug button** (GPIO5)
2. ESP32 creates its own WiFi network: **`Orion-Gateway`**
3. Connect to the network with password: **`orion12345`**
4. Open browser and go to: **`http://192.168.4.1`**

### HTTP Configuration Interface

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web interface with links to all endpoints |
| `/api/status` | GET | Full system status (JSON) |
| `/api/devices` | GET | List of online devices (JSON) |
| `/api/config` | GET | Current configuration (JSON) |
| `/api/config` | POST | Update configuration (e.g., `global_key=5A`) |
| `/api/mode` | GET | Current mode and master detection status |

**Example - Change encryption key:**
```bash
curl -X POST http://192.168.4.1/api/config -d "global_key=5A"
```

### WiFi AP Behavior

- **WiFi starts** when debug button pressed (any debug mode)
- **WiFi stays on** until device reboot
- **LED indicator**: Debug LED slow blink (500ms) when AP active
- **Default IP**: `192.168.4.1` (standard ESP32 AP address)
- **Performance impact**: Minimal (~0.5-1ms added to loop when HTTP requests active)

## Dual-Mode Operation (MONITOR / MASTER)

The firmware supports two operating modes in a single project:

### MONITOR Mode (Default)

- **Passive** — DE pin held LOW, ESP32 only receives
- **Safe** — zero bus interference, works alongside С2000М
- **Auto-detects** С2000М master traffic on the bus
- This is the default mode on every boot

### MASTER Mode

- **Active** — ESP32 acts as С2000М replacement
- **Polls devices** round-robin (ping, read type, read status, read events)
- **Sends commands** — arm/disarm zones, reset alarms
- **Use only** when С2000М PC master is **disconnected** from the bus

### Safety Mechanisms

1. **Boots in MONITOR** — always starts passive, never transmits on boot
2. **Master detection** — monitors bus for С2000М request packets
3. **Refuses MASTER switch** — if С2000М traffic seen within last 30 seconds
4. **Emergency fallback** — if С2000М traffic detected while in MASTER mode, immediately switches back to MONITOR (prevents bus collisions)
5. **Mode change notifications** — sent to serial cable when mode changes

### Physical Mode Switch (Hardware)

**GPIO8 — SPDT Toggle Switch**

The gateway includes a **physical hardware switch** for mode selection:

- **Switch DOWN** (connected to GND) → **MONITOR mode** (safe, passive)
- **Switch UP** (open circuit) → **MASTER mode** (active control)

**Wiring:**
```
GPIO8 ──┬── [10kΩ pull-up to 3.3V]
        │
        └── [SPDT Toggle Switch] ──┬── GND (MONITOR position)
                                   └── Open (MASTER position)
```

**Behavior:**
- Mode changes automatically when switch is toggled
- Still respects safety: won't enter MASTER if С2000М detected
- Debounced (500ms) to prevent false triggers
- Notifications sent to serial: `MODE_SWITCH,MONITOR` or `MODE_SWITCH,MASTER`

**Use case:** Install the switch on your enclosure for quick mode changes without software commands.

### Mode Switching Commands (Serial — Alternative to Physical Switch)

| Command | Action |
|---------|--------|
| `MASTER_ON` | Switch to MASTER mode (refused if С2000М detected) |
| `MASTER_OFF` | Switch back to MONITOR mode |
| `MASTER_STATUS` | Show current mode, master detection state, TX stats |
| `POLL_ON` | Enable auto-polling of devices (MASTER mode) |
| `POLL_OFF` | Disable auto-polling (manual commands only) |

### Master Commands (Serial — only work in MASTER mode)

| Command | Action |
|---------|--------|
| `ARM,<addr>,<zone>` | Arm zone on device |
| `DISARM,<addr>,<zone>` | Disarm zone on device |
| `RESET_ALARM,<addr>,<zone>` | Reset alarm on zone |
| `PING_DEV,<addr>` | Ping a specific device |
| `SCAN` | Scan entire bus (addresses 1-127) |

**Examples:**
```
# Check current mode
MASTER_STATUS
→ MODE,MONITOR,master_detected=1,last_master=5s,tx=0,tx_err=0,timeouts=0

# С2000М is active — cannot switch to MASTER
MASTER_ON
→ ERR,MASTER_ON,master_detected_5s_ago

# Disconnect С2000М from bus... wait 30 seconds...
MASTER_STATUS
→ MODE,MONITOR,master_detected=0,last_master=35s,tx=0,tx_err=0,timeouts=0

# Now safe to switch
MASTER_ON
→ OK,MASTER_ON

# Arm zone 1 on device 5
ARM,5,1
→ OK,ARM,5,1

# Scan the bus for all devices
SCAN
→ SCAN,START
→ SCAN,FOUND,1
→ SCAN,FOUND,2
→ SCAN,FOUND,5
→ SCAN,DONE

# Switch back to passive monitoring
MASTER_OFF
→ OK,MASTER_OFF
```

### LED Indicators

| LED | State | Meaning |
|-----|-------|---------|
| **Status (green)** | Fast blink (200ms) | Devices online, bus active |
| **Status (green)** | Slow blink (1000ms) | No devices found yet |
| **Status (green)** | Solid ON | Alarm detected |
| **Debug (red)** | OFF | MONITOR mode, no debug, WiFi OFF |
| **Debug (red)** | Solid ON | Debug mode active (raw/verbose) |
| **Debug (red)** | Slow blink (500ms) | **WiFi AP active** |
| **Debug (red)** | Fast blink (100ms) | **MASTER mode active** |

## Configuration

Edit the defines at the top of `src/main.cpp`:

```cpp
/* WiFi Access Point (on-demand) */
#define WIFI_AP_SSID      "Orion-Gateway"   /* AP network name */
#define WIFI_AP_PASSWORD  "orion12345"      /* AP password (min 8 chars) */
#define WIFI_AP_CHANNEL   6                 /* WiFi channel (1-13) */

/* Orion bus */
#define ORION_GLOBAL_KEY  0x00   /* Set to your system's encryption key */

/* RS-485 pins (UART1) */
#define RS485_TX_PIN   7     /* GPIO7 - UART1 TX */
#define RS485_RX_PIN   6     /* GPIO6 - UART1 RX */
#define RS485_DE_PIN   2     /* GPIO2 - MAX3485 DE+RE */

/* External serial cable (UART0) */
#define EXT_SERIAL_TX_PIN  21   /* GPIO21 */
#define EXT_SERIAL_RX_PIN  20   /* GPIO20 */
#define EXT_SERIAL_BAUD    115200
```

The encryption key can also be changed at runtime via:
- Serial command: `SET_KEY,5A`
- HTTP API: `POST /api/config` with `global_key=5A`

## Building & Flashing

### PlatformIO (recommended)

```bash
cd firmware/esp32_orion_slave
pio run                    # Build
pio run --target upload    # Flash
pio device monitor         # Serial debug output
```

### Arduino IDE

1. Install ESP32 board support
2. Copy `lib/OrionSlave/` files into your Arduino libraries folder
3. Open `src/main.cpp`, rename to `.ino`
4. Install `PubSubClient` library
5. Select "ESP32 Dev Module" board
6. Upload

## Serial Cable Protocol

The external system connects via UART1 (GPIO25/GPIO26, 115200 baud, 8N1).

### Automatic Messages (ESP32 → External System)

The ESP32 **pushes** these messages automatically when events occur:

| Message | When | Format |
|---------|------|--------|
| `BOOT` | On startup/reboot | `BOOT,<version>,<boot_count>,<ip_or_no_wifi>` |
| `HEARTBEAT` | Every 5 seconds | `HEARTBEAT,<uptime_s>,<packets>,<online>,<crc_err>,<heap>` |
| `STATUS` | Zone status changes | `STATUS,<dev>,<zone>,<code>,<string>` |
| `DEVICE` | Device online/offline | `DEVICE,<dev>,<ONLINE\|OFFLINE>,<type>,<type_str>` |
| `EVENT` | Bus event captured | `EVENT,<dev>,<zone>,<code>,<string>` |
| `RAW` | Debug mode only | `RAW,<REQ\|RSP>,<dev>,<ENC\|CLR>,<cmd>,<len>,<hex>` |

**Examples:**
```
BOOT,1.0.0,3,192.168.1.50       # Firmware 1.0.0, 3rd boot, WiFi connected
HEARTBEAT,120,5340,12,0,180000  # Uptime 120s, 5340 packets, 12 devices online
STATUS,1,3,4,armed              # Device 1, zone 3 armed
STATUS,2,1,9,alarm              # Device 2, zone 1 ALARM!
DEVICE,1,ONLINE,53,Signal-20M   # Device 1 came online
EVENT,1,5,9,alarm               # Alarm event logged
RAW,REQ,1,ENC,0x57,05,A5B3C2... # Raw packet (debug mode)
```

### Commands (External System → ESP32)

Send commands as text lines ending with `\n`:

| Command | Response | Description |
|---------|----------|-------------|
| `PING` | `PONG,<packets>,<online>` | Health check |
| `VERSION` | `VERSION,<ver>,<uptime>,<boots>,<heap>,<online>` | Firmware info |
| `GET_STATUS` | `SYSTEM,{json}` | Full system status |
| `GET_DEVICE,<addr>` | `DEVICE_INFO,{json}` | Single device detail |
| `GET_EVENTS` | `EVENTS,<count>,[...]` | Recent event log |
| `DEBUG_ON` | `OK,DEBUG_ON` | Enable raw packet hex dump |
| `DEBUG_OFF` | `OK,DEBUG_OFF` | Disable raw packet dump |
| `SET_KEY,<hex>` | `OK,SET_KEY,<hex>` | Set encryption key (saved to flash) |
| `REBOOT` | `OK,REBOOT` | Restart ESP32 |

**Examples:**
```bash
# Health check
PING
→ PONG,5340,12

# Get firmware version
VERSION
→ VERSION,1.0.0,120,3,180000,12

# Set encryption key to 0x5A
SET_KEY,5A
→ OK,SET_KEY,5A

# Enable debug mode
DEBUG_ON
→ OK,DEBUG_ON
```

## MQTT Topics

### Published by gateway

| Topic | Payload | Frequency |
|-------|---------|-----------|
| `orion/system` | System summary JSON | Every 10s |
| `orion/device/<addr>/state` | Device state JSON | Every 10s |
| `orion/device/<addr>/online` | `{"online":true/false}` | On change (retained) |
| `orion/device/<addr>/<zone>/status` | Zone status change JSON | On change |
| `orion/device/<addr>/alarm` | Alarm detail JSON | On alarm |
| `orion/event/<addr>` | Event JSON | On event |

### System JSON example

```json
{
  "online_devices": 4,
  "total_packets": 15230,
  "crc_errors": 2,
  "uptime": 3600,
  "free_heap": 180000
}
```

### Device state JSON example

```json
{
  "addr": 1,
  "online": true,
  "type": 52,
  "type_s": "Signal-20P",
  "fw": "3.16",
  "zones": 4,
  "zst": [
    {"z": 1, "s": 4, "s_str": "armed"},
    {"z": 2, "s": 6, "s_str": "disarmed"},
    {"z": 3, "s": 9, "s_str": "alarm"},
    {"z": 4, "s": 6, "s_str": "disarmed"}
  ]
}
```

## HTTP REST API

Available when WiFi is connected. Default port 80.

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | HTML index page with links |
| `/api/status` | GET | Full system status JSON |
| `/api/devices` | GET | List of online devices |
| `/api/device/<addr>` | GET | Single device detail |
| `/api/events` | GET | Recent event log |
| `/api/config` | GET | Get current configuration |
| `/api/config` | POST | Update configuration |

### Configuration via WiFi

**Get current configuration:**
```bash
curl http://192.168.1.50/api/config
```

**Response:**
```json
{
  "packet_timeout_ms": 5,
  "response_gap_ms": 50,
  "device_timeout_ms": 10000,
  "poll_interval_ms": 0,
  "orion_baud": 9600,
  "global_key": 0,
  "mqtt_enabled": true,
  "http_enabled": true,
  "ota_enabled": true
}
```

**Update configuration:**
```bash
# Change encryption key
curl -X POST http://192.168.1.50/api/config \
  -d "global_key=90"

# Adjust protocol timing (if experiencing packet loss)
curl -X POST http://192.168.1.50/api/config \
  -d "packet_timeout_ms=10&response_gap_ms=100"

# Reduce device timeout for faster offline detection
curl -X POST http://192.168.1.50/api/config \
  -d "device_timeout_ms=5000"
```

**Configuration parameters:**

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `packet_timeout_ms` | 5 | 1-100 | Inter-byte timeout for packet framing |
| `response_gap_ms` | 50 | 10-500 | Gap to distinguish request from response |
| `device_timeout_ms` | 10000 | 1000-60000 | Device offline timeout (ms) |
| `poll_interval_ms` | 0 | 0-1000 | Minimum time between poll() calls |
| `global_key` | 0 | 0-255 | Encryption key (0x00-0xFF) |

**All settings are saved to flash and persist across reboots.**

## Connecting to External System

### Option A: Direct TTL serial (3.3V MCU)
Wire GPIO25→RX, GPIO26←TX, shared GND. Works with STM32, Arduino, Raspberry Pi.

### Option B: USB-UART adapter (to PC)
Use CP2102/CH340 module. GPIO25→adapter RX, GPIO26←adapter TX. Appears as COM port.

### Option C: RS-232 (industrial)
Add MAX3232 level shifter + DB9 connector. For PLC/SCADA integration.

### Option D: WiFi only
Don't connect UART1 at all. Use MQTT and/or HTTP REST API exclusively.

## Production Features

### Watchdog Timer

- **30-second timeout** — ESP32 auto-restarts if firmware hangs
- Prevents permanent lockups from bus noise or WiFi issues
- Watchdog is fed every loop iteration

### Heartbeat Monitoring

- **Sent every 5 seconds** via serial cable
- External system can detect if ESP32 crashes or cable disconnects
- Includes uptime, packet count, online devices, errors, free memory

### LED Status Indicators

#### Status LED (Green, GPIO2)

| Pattern | Meaning |
|---------|----------|
| **Fast blink** (200ms) | Bus active, devices online |
| **Slow blink** (1000ms) | Searching for devices |
| **Solid ON** | Alarm detected on any zone |
| **OFF** | No bus activity |

#### Debug LED (Red, GPIO15 - Optional)

| Pattern | Meaning |
|---------|----------|
| **Solid ON** | Debug mode active (raw or verbose) |
| **OFF** | Debug mode off (normal operation) |

### Debug Button (GPIO0 / BOOT Button)

**Press the BOOT button to cycle through debug modes:**

1. **Press 1**: All debug OFF
2. **Press 2**: Raw packet dump ON (hex dump of every packet)
3. **Press 3**: Verbose logging ON (detailed stats every 10s)
4. **Press 4**: Both raw + verbose (full debug mode)
5. **Press 5**: Cycles back to OFF

**What happens when you press the button:**
- Debug LED changes state (if connected)
- Message printed to USB serial
- Message sent to external serial cable: `DEBUG,OFF|RAW|VERBOSE|FULL`
- If WiFi-on-demand mode enabled, WiFi starts when debug mode activated

**Use cases:**
- **Field debugging**: Press button to enable debug without connecting USB
- **Protocol verification**: See raw hex packets to verify decoding
- **Performance monitoring**: Check memory, packet stats, WiFi signal
- **WiFi on-demand**: Save power by only enabling WiFi when debugging

### Raw Debug Mode

**Critical for initial setup and protocol verification.**

Enable with `DEBUG_ON` command. Every packet on the bus is dumped as hex:

```
RAW,REQ,1,ENC,0x57,05,A5B3C2D4E5
RAW,RSP,1,ENC,0x57,08,5A3F2E1D0C9B8A79
```

Use this to:
- Verify the ESP32 is receiving bus traffic
- Discover the encryption key (see below)
- Debug protocol decoding issues
- Confirm CRC validation is working

### NVS Config Storage

- Encryption key saved to flash with `SET_KEY` command
- Boot counter persists across reboots
- Config survives power loss

### OTA Firmware Updates

**Update firmware over WiFi without USB cable.**

When WiFi is connected, the ESP32 advertises as `orion-gateway` on the network.

**Using Arduino IDE:**
1. Tools → Port → Network Ports → `orion-gateway`
2. Upload as normal

**Using PlatformIO:**
```bash
pio run --target upload --upload-port orion-gateway.local
```

**Using Python:**
```bash
python -m espota -i 192.168.1.50 -f firmware.bin
```

### WiFi Auto-Reconnect

- Retries every 30 seconds if WiFi drops
- Gateway continues working via serial cable during WiFi outage
- MQTT reconnects automatically when WiFi returns

## Discovering the Encryption Key

**Most Orion systems use the default key `0x00` (no encryption).** The firmware is pre-configured with this.

If devices don't appear after 30 seconds, the key might be different.

### Method 1: Sniff the Key from Bus Traffic

1. **Power off the entire Orion system** (turn off С2000М)
2. **Enable debug mode**: Send `DEBUG_ON` via serial cable
3. **Power on С2000М**
4. **Watch for the SET_GLOBAL_KEY command** in the raw packet dump:

```
RAW,REQ,0,CLR,0x11,02
                    ^^
                    This is the key! (0x00 in this example)
```

Command `0x11` is `SET_GLOBAL_KEY`. The byte after the command is the encryption key.

**Examples:**
```
RAW,REQ,0,CLR,0x11,00  → Key is 0x00 (default)
RAW,REQ,0,CLR,0x11,5A  → Key is 0x5A
RAW,REQ,0,CLR,0x11,FF  → Key is 0xFF
```

5. **Set the key**: `SET_KEY,5A` (saved to flash, survives reboot)

### Method 2: Check Bolid Desktop App

If you have access to the Bolid configuration software (Uprog/PProg):

1. Open system configuration
2. Look for "Encryption" or "Global Key" settings
3. The key is shown as a hex value (00-FF)

### Method 3: Try Common Keys

| Key | Hex | Usage |
|-----|-----|-------|
| Default | `0x00` | 90% of systems |
| Test | `0x5A` | Common test value |
| Max | `0xFF` | Sometimes used |

Try each with `SET_KEY,<hex>` and check if devices appear with `GET_STATUS`.

### Method 4: Brute Force (Last Resort)

Try all 256 possible keys:

```python
import serial
import time

ser = serial.Serial('COM3', 115200)

for key in range(256):
    ser.write(f'SET_KEY,{key:02X}\n'.encode())
    time.sleep(2)
    ser.write(b'GET_STATUS\n')
    response = ser.read(1000)
    
    if b'"count":' in response and b'"count":0' not in response:
        print(f'KEY FOUND: 0x{key:02X}')
        break
```

## Testing with Real Hardware

The protocol decoding is based on reverse-engineered documentation. When you connect to real С2000М + devices:

1. **Start with `DEBUG_ON`** — verify you're receiving packets
2. **Check CRC errors** — should be 0 or very low (<1%)
3. **Wait 30 seconds** — devices should appear in `GET_STATUS`
4. **If no devices found**:
   - Check encryption key (see above)
   - Verify RS-485 wiring (A to A, B to B)
   - Check baud rate (should be 9600)
   - Look for bus termination resistors (120Ω at each end)

5. **Trigger a test alarm** — verify `STATUS` and `EVENT` messages appear
6. **Disable debug mode** — `DEBUG_OFF` for normal operation

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No `BOOT` message on serial | Check UART wiring, baud rate (115200) |
| No `HEARTBEAT` | ESP32 crashed — check USB serial for errors |
| `HEARTBEAT` but no devices | Wrong encryption key, bad RS-485 wiring, or no bus traffic |
| High CRC errors | Bus noise, termination issues, wrong baud rate |
| Devices appear then disappear | Encryption key changed mid-session |
| LED always off | No bus traffic detected |
| LED solid on | Alarm condition on a zone |
| WiFi won't connect | Check SSID/password in `main.cpp` |
| OTA not visible | WiFi not connected, or firewall blocking mDNS |

## Disclaimer

Based on reverse-engineered Bolid Orion protocol. **Not officially supported by НВП "Болид"**. The passive listener approach is safe — it never transmits and cannot interfere with the bus. However, the accuracy of decoded data depends on knowing the correct encryption key and may require tuning for specific firmware versions. Always test thoroughly with your specific hardware before production deployment.
