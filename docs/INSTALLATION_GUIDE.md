# ESP32-C3 Orion Gateway — Installation & Integration Guide

## Complete Step-by-Step Deployment Manual

**Version:** 2.0  
**Last Updated:** March 11, 2026  
**Target Device:** ESP32-C3 Super Mini with ADM2483 Isolated RS-485

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Pre-Installation Checklist](#pre-installation-checklist)
3. [Hardware Assembly](#hardware-assembly)
4. [Firmware Installation](#firmware-installation)
5. [Initial Testing (Bench Test)](#initial-testing-bench-test)
6. [Site Integration](#site-integration)
7. [Configuration](#configuration)
8. [Verification & Commissioning](#verification--commissioning)
9. [Troubleshooting](#troubleshooting)
10. [Maintenance](#maintenance)

---

## Prerequisites

### Required Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **Laptop/PC with USB port** | Programming & monitoring | Windows/Linux/macOS |
| **USB-C cable (data capable)** | Power + programming | NOT charge-only cable |
| **Multimeter** | Voltage testing | Basic model sufficient |
| **Small screwdriver set** | Screw terminals | Phillips + flathead |
| **Wire strippers** | RS-485 cable prep | 18-24 AWG |
| **Label maker (optional)** | Cable identification | Recommended for multiple units |

### Required Software

| Software | Version | Download Link |
|----------|---------|---------------|
| **PlatformIO** | Latest | https://platformio.org/install |
| **VS Code** | Latest | https://code.visualstudio.com/ |
| **USB-to-Serial Driver** | Latest | Auto-installed with PlatformIO |
| **Serial Monitor** | Any | PuTTY, Arduino IDE, or PlatformIO built-in |

### Required Components

Refer to `docs/BOM_complete.md` for complete list. Key items:

- ✅ ESP32-C3 Super Mini board
- ✅ ADM2483 isolated RS-485 module
- ✅ AMS1117-3.3 LDO (if custom PCB)
- ✅ Resistors, capacitors, LEDs, switches
- ✅ Screw terminal for RS-485 connection
- ✅ Twisted pair cable for RS-485 (shielded recommended)

### Site Information Required

Before installation, gather this information:

| Information | Example | Where to Find |
|-------------|---------|---------------|
| Orion panel model | C2000-ADEM | Panel label |
| RS-485 bus voltage | 12V or 24V | Panel documentation |
| Existing device count | 15 devices | Panel configuration |
| RS-485 bus location | Terminal block TB3 | Wiring diagram |
| Network requirements | WiFi AP or USB serial | Site IT policy |

---

## Pre-Installation Checklist

### Safety First

- [ ] **Read Orion system manual** — Understand your specific panel
- [ ] **Verify power is OFF** — Disconnect Orion panel from mains before wiring
- [ ] **Check for existing RS-485 devices** — Note termination resistor locations
- [ ] **Confirm USB power source** — Dedicated PC/laptop or USB power adapter
- [ ] **Prepare workspace** — Clean, static-free area for assembly

### Documentation Review

- [ ] Read `docs/esp32c3_schematic_final.md` — Circuit diagram
- [ ] Read `docs/BOM_complete.md` — Component list
- [ ] Read `firmware/esp32_orion_slave/README.md` — Firmware features
- [ ] Print this installation guide for reference

---

## Hardware Assembly

### Step 1: Assemble the Gateway Board

#### Option A: Using Breadboard (Prototype)

1. **Insert ESP32-C3 Super Mini** into breadboard
2. **Connect AMS1117-3.3 LDO:**
   - VIN → ESP32 5V pin
   - VOUT → 3.3V rail (separate from ESP32 3.3V pin)
   - GND → Common GND
3. **Add capacitors:**
   - C12 (10µF) on AMS1117 input
   - C13 (22µF) on AMS1117 output
4. **Mount ADM2483 module** on breadboard
5. **Wire ADM2483 to ESP32:**
   - VCC → 3.3V_EXT (AMS1117 output)
   - GND → Common GND
   - RO → GPIO6
   - DI → GPIO7
   - DE → GPIO2
   - RE → GPIO2 (tie to DE)
6. **Add LED circuits:**
   - Status LED (green): GPIO3 → R6 (220Ω) → Q1 base, Q1 collector → LED1 cathode, LED1 anode → R7 (1kΩ) → 3.3V_EXT
   - Debug LED (red): GPIO4 → R8 (220Ω) → Q2 base, Q2 collector → LED2 cathode, LED2 anode → R9 (1kΩ) → 3.3V_EXT
   - Power LED (yellow): 3.3V_EXT → R10 (1kΩ) → LED3 anode, LED3 cathode → GND
7. **Add buttons and mode switch:**
   - Reset: 3.3V_EXT → R3 (10kΩ) → EN pin, SW1 between EN and GND
   - Debug: 3.3V_EXT → R4 (10kΩ) → GPIO5, SW2 between GPIO5 and GND, C10 (100nF) across SW2
   - Mode: 3.3V_EXT → R11 (10kΩ) → GPIO8, SW3 between GPIO8 and GND

#### Option B: Using Custom PCB

1. **Solder AMS1117-3.3** (watch orientation — tab is GND)
2. **Solder C12, C13** close to AMS1117
3. **Solder pin headers** for ESP32-C3 Super Mini
4. **Insert ESP32-C3** into headers
5. **Mount ADM2483 module** via pin headers or direct solder
6. **Solder transistors Q1, Q2** (watch EBC pinout)
7. **Solder resistors R3, R4, R6-R11**
8. **Solder LEDs** (watch polarity!)
9. **Solder switches SW1, SW2, SW3**
10. **Solder capacitor C10**
11. **Solder screw terminal J2** for RS-485
12. **Solder pin header J1** for external serial (optional)

### Step 2: Visual Inspection

- [ ] **No solder bridges** — Check all pins with magnifier
- [ ] **Correct polarity** — LEDs, capacitors, transistors
- [ ] **Solid joints** — No cold solder joints
- [ ] **Clean flux** — Remove flux residue with isopropyl alcohol

### Step 3: Power-On Test (No RS-485 Connected)

1. **Connect USB-C cable** to ESP32-C3
2. **Connect USB to PC**
3. **Check power LED (yellow)** — Should light immediately
4. **Measure voltages:**
   - ESP32 5V pin: **4.8-5.2V** ✅
   - AMS1117 output (3.3V_EXT): **3.25-3.35V** ✅
   - ESP32 3.3V pin: **3.25-3.35V** ✅
5. **If voltages incorrect:**
   - Check USB cable (must be data-capable)
   - Check AMS1117 orientation
   - Check for shorts on 3.3V_EXT rail

---

## Firmware Installation

### Step 1: Install Development Environment

#### Windows

```powershell
# Install VS Code
winget install -e --id Microsoft.VisualStudioCode

# Install PlatformIO extension
# Open VS Code → Extensions → Search "PlatformIO IDE" → Install
```

#### Linux/macOS

```bash
# Install VS Code from https://code.visualstudio.com/
# Install PlatformIO extension from VS Code Extensions panel
```

### Step 2: Clone/Download Firmware

```bash
cd d:\Projects\perimiter_security\firmware\esp32_orion_slave
```

Or download from your repository.

### Step 3: Open Project in PlatformIO

1. **Open VS Code**
2. **File → Open Folder**
3. **Navigate to:** `d:\Projects\perimiter_security\firmware\esp32_orion_slave`
4. **Wait for PlatformIO** to initialize (first time takes 2-5 minutes)

### Step 4: Verify Configuration

Check `platformio.ini`:

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
```

### Step 5: Build Firmware

1. **Click PlatformIO icon** (alien head) in left sidebar
2. **Click "Build"** under PROJECT TASKS
3. **Wait for compilation** (1-3 minutes first time)
4. **Check for errors** — Should show "SUCCESS"

### Step 6: Upload Firmware

1. **Connect ESP32-C3 via USB-C**
2. **Hold BOOT button** on ESP32-C3 (if upload fails)
3. **Click "Upload"** in PlatformIO
4. **Wait for upload** (30-60 seconds)
5. **Success message** should appear

**If upload fails:**
- Try different USB port
- Install CH340/CP2102 drivers manually
- Hold BOOT button during upload
- Check USB cable (must support data)

### Step 7: Verify Firmware

1. **Click "Monitor"** in PlatformIO
2. **Press RESET button** on ESP32-C3
3. **Check boot message:**

```
[BOOT] ESP32-C3 Orion Gateway v1.0.0
[BOOT] USB serial ready
[BOOT] WiFi AP available on-demand (press debug button)
[BOOT] RS-485 bus: 9600 baud, GPIO6/7
[BOOT] Mode: MONITOR (passive listening)
[READY] System initialized
```

4. **If no output:**
   - Check serial monitor baud rate (115200)
   - Try different USB cable
   - Press RESET button

---

## Initial Testing (Bench Test)

### Test 1: LED Indicators

| LED | Expected State | Action |
|-----|----------------|--------|
| **Yellow (Power)** | Always ON | If OFF: Check 3.3V_EXT voltage |
| **Green (Status)** | Blinking slowly | If OFF: Check Q1, R6, R7, LED1 |
| **Red (Debug/Mode)** | OFF initially | Will light when debug mode active |

### Test 2: Debug Button

1. **Press debug button (SW2)** once
2. **Red LED should blink fast** (debug mode active)
3. **Serial monitor shows:** `[DEBUG] Debug mode: RAW (hex dump)`
4. **Press again** to cycle modes: RAW → VERBOSE → FULL → OFF

### Test 3: Mode Switch

1. **Toggle mode switch (SW3)** to GND position
2. **Serial monitor shows:** `[MODE] Switched to MONITOR mode (passive)`
3. **Toggle to open position**
4. **Serial monitor shows:** `[MODE] Switched to MASTER mode (active control)`
5. **Return to MONITOR** for safe testing

### Test 4: WiFi AP (On-Demand)

1. **Press and hold debug button** for 3 seconds
2. **Red LED slow blink** (WiFi AP active)
3. **Serial monitor shows:**
   ```
   [WiFi] Starting Access Point...
   [WiFi] SSID: Orion-Gateway
   [WiFi] Password: orion12345
   [WiFi] IP: 192.168.4.1
   [HTTP] Server started on port 80
   ```
4. **Connect phone/laptop to WiFi:**
   - SSID: `Orion-Gateway`
   - Password: `orion12345`
5. **Open browser:** `http://192.168.4.1`
6. **Check API endpoints:**
   - `http://192.168.4.1/api/status` — Should show JSON status
   - `http://192.168.4.1/api/devices` — Empty array (no RS-485 connected yet)

**If WiFi doesn't start:**
- Check antenna clearance (5mm around ESP32-C3)
- Try different debug button hold duration
- Check serial monitor for error messages

---

## Site Integration

### Step 1: Identify RS-485 Connection Points

**Orion Panel RS-485 Bus Location:**

Typical locations (check your panel manual):
- **Terminal block TB3** (A, B, GND)
- **Expansion port** (labeled RS-485)
- **Existing device daisy-chain** (parallel tap)

**Important:** Do NOT disconnect existing devices. The gateway taps into the bus in parallel.

### Step 2: Prepare RS-485 Cable

1. **Use twisted pair cable** (Cat5e works, or dedicated RS-485 cable)
2. **Strip 5-7mm** from each wire end
3. **Tin wire ends** (optional but recommended)
4. **Label wires:**
   - Wire 1: **A** (usually green or white/orange in Cat5e)
   - Wire 2: **B** (usually brown or orange in Cat5e)

**Cable length guidelines:**
- <10m: Any twisted pair
- 10-100m: Shielded twisted pair recommended
- >100m: Use proper RS-485 cable with 120Ω characteristic impedance

### Step 3: Connect to Orion RS-485 Bus

**⚠️ CRITICAL SAFETY:**
- **Power OFF** the Orion panel before wiring
- **Verify voltage** with multimeter before connecting
- **Do NOT reverse A/B** — can damage ADM2483

**Connection procedure:**

1. **Power OFF Orion panel** (disconnect from mains)
2. **Locate RS-485 terminals** (A, B, GND)
3. **Connect to gateway J2:**
   - Orion **A** → Gateway J2 Pin 1 (A)
   - Orion **B** → Gateway J2 Pin 2 (B)
4. **Do NOT connect GND** — ADM2483 is isolated (GND_ISO should float)
5. **Tighten screw terminals** firmly
6. **Verify connections** with multimeter (continuity test)

**Termination resistor:**
- **If gateway is at bus end:** Add 120Ω resistor across A-B on ADM2483 module (check module datasheet)
- **If gateway is mid-bus:** No termination needed

### Step 4: Power Up Sequence

1. **Connect gateway USB-C** to PC/laptop
2. **Open serial monitor** (115200 baud)
3. **Power ON Orion panel**
4. **Wait 30 seconds** for Orion system boot
5. **Watch serial monitor** for packet reception

**Expected output:**

```
[READY] System initialized
[RS485] Packet received: 12 bytes from device 0x01
[DEVICE] Device 0x01 ONLINE (C2000-ADEM panel)
[RS485] Packet received: 8 bytes from device 0x05
[DEVICE] Device 0x05 ONLINE (S2000-KDL keypad)
```

**If no packets received:**
- Check A/B wiring (try swapping)
- Verify Orion panel is powered and active
- Check RS-485 baud rate (default 9600, configurable)
- Measure voltage on A-B (should see differential signal)

---

## Configuration

### Via Serial Commands

Connect to USB serial (115200 baud) and send commands:

#### Check Current Configuration

```
GET_CONFIG
```

Response:
```
CONFIG,9600,5,50,10000,0
# Format: baud,packet_timeout,response_gap,device_timeout,poll_interval
```

#### Change RS-485 Baud Rate

```
SET_BAUD,9600
```

**Note:** Must match Orion panel baud rate (usually 9600).

#### Enable MASTER Mode (Active Control)

**⚠️ WARNING:** Only use MASTER mode if you understand Orion protocol!

```
MASTER_ON
```

Toggle mode switch to MASTER position, or send command.

#### Scan for Devices

```
SCAN
```

Response:
```
SCAN_START
DEVICE,0x01,ONLINE,C2000-ADEM,Panel
DEVICE,0x05,ONLINE,S2000-KDL,Keypad
DEVICE,0x10,ONLINE,S2000-SP1,Zone expander
SCAN_COMPLETE,3
```

### Via WiFi AP HTTP API

1. **Start WiFi AP** (hold debug button 3 seconds)
2. **Connect to:** `Orion-Gateway` / `orion12345`
3. **Open browser:** `http://192.168.4.1`

#### API Endpoints

**GET `/api/status`** — System status
```json
{
  "uptime": 12345,
  "mode": "MONITOR",
  "devices_online": 3,
  "packets_received": 1523,
  "crc_errors": 2
}
```

**GET `/api/devices`** — All discovered devices
```json
[
  {"addr": 1, "type": "C2000-ADEM", "online": true},
  {"addr": 5, "type": "S2000-KDL", "online": true}
]
```

**POST `/api/config`** — Update configuration
```json
{
  "baud": 9600,
  "packet_timeout": 5,
  "response_gap": 50
}
```

**GET `/api/mode`** — Current mode
```json
{"mode": "MONITOR"}
```

**POST `/api/mode`** — Change mode
```json
{"mode": "MASTER"}
```

---

## Verification & Commissioning

### Verification Checklist

- [ ] **Power LED (yellow) ON** — 3.3V_EXT rail working
- [ ] **Status LED (green) blinking** — Firmware running
- [ ] **Serial monitor shows boot message** — USB communication OK
- [ ] **RS-485 packets received** — Bus connection OK
- [ ] **Devices discovered** — Protocol decoding OK
- [ ] **WiFi AP starts** — WiFi hardware OK
- [ ] **HTTP API responds** — Web server OK
- [ ] **Mode switch works** — MONITOR ↔ MASTER switching OK
- [ ] **Debug button cycles modes** — Button input OK

### Performance Metrics

Check serial monitor for statistics:

```
[STATS] Uptime: 3600s | Packets: 1523 | CRC errors: 2 (0.13%)
[STATS] Devices: 3 online, 0 offline | Loop: 0.4ms avg
```

**Good performance:**
- CRC errors: <1%
- Loop time: <1ms
- Devices online: Matches expected count

**Poor performance indicators:**
- CRC errors: >5% — Check wiring, cable quality, termination
- Loop time: >5ms — Firmware issue, check debug output
- Devices offline: Check Orion panel, device power

### Long-Term Monitoring

**First 24 hours:**
- Monitor CRC error rate
- Check device online/offline transitions
- Verify no unexpected resets (check uptime)

**First week:**
- Log all events to file
- Verify event timestamps match Orion panel
- Check for memory leaks (uptime should be continuous)

---

## Troubleshooting

### Problem: No Power LED

**Symptoms:** Yellow LED off, no USB serial

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| USB cable | Try different cable | Use data-capable USB-C cable |
| USB port | Try different port | Use USB 2.0 or 3.0 port |
| AMS1117 reversed | Check orientation | Tab is GND, resolder correctly |
| Short circuit | Measure resistance | Check for solder bridges |

### Problem: No RS-485 Packets

**Symptoms:** Serial monitor shows no packets, devices not discovered

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| A/B swapped | Swap wires | Try reversing A and B connections |
| Wrong baud rate | Check panel config | Send `SET_BAUD,9600` or match panel |
| ADM2483 not powered | Measure VCC | Check 3.3V_EXT on ADM2483 VCC pin |
| Orion panel off | Check panel | Power on Orion panel |
| Cable break | Continuity test | Replace cable |
| Wrong GPIO pins | Check wiring | Verify GPIO6=RO, GPIO7=DI, GPIO2=DE/RE |

### Problem: High CRC Errors (>5%)

**Symptoms:** Many packets with CRC failures

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| Noise on bus | Measure signal | Use shielded twisted pair cable |
| Missing termination | Check bus ends | Add 120Ω resistor at bus ends |
| Cable too long | Measure length | Use RS-485 repeater if >1000m |
| Ground loop | Check isolation | Verify ADM2483 GND_ISO not connected to ESP32 GND |
| Baud rate mismatch | Check config | Match Orion panel baud rate exactly |

### Problem: WiFi AP Won't Start

**Symptoms:** Red LED doesn't slow blink, no WiFi network visible

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| Antenna blocked | Check clearance | Keep 5mm clear zone around ESP32-C3 |
| Debug button not held | Hold longer | Hold debug button for 3+ seconds |
| Firmware issue | Check serial | Look for WiFi error messages |
| ESP32 reset | Check uptime | If resets, check power supply (try USB 3.0 port) |

### Problem: Device Resets Randomly

**Symptoms:** Uptime counter resets, boot messages repeat

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| Insufficient power | Try USB 3.0 port | USB 3.0 provides 900mA vs 500mA |
| Watchdog timeout | Check serial | Look for "WDT reset" messages |
| Brown-out | Measure voltage | Check 3.3V rail stays >3.2V |
| Firmware crash | Check debug output | Enable verbose debug mode |

### Problem: Mode Switch No Effect

**Symptoms:** Toggling SW3 doesn't change mode

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| R11 missing | Check pull-up | Verify 10kΩ resistor to 3.3V_EXT |
| GPIO8 not connected | Continuity test | Check wiring to GPIO8 |
| Switch broken | Test with multimeter | Replace switch |
| Firmware issue | Check serial | Look for mode change messages |

### Problem: HTTP API Not Responding

**Symptoms:** WiFi connects but browser shows timeout

**Causes & Solutions:**

| Cause | Check | Solution |
|-------|-------|----------|
| Wrong IP | Check serial output | Use IP shown in boot message (usually 192.168.4.1) |
| Firewall | Disable temporarily | Check PC firewall settings |
| Browser cache | Clear cache | Try incognito/private mode |
| Server crashed | Check serial | Look for error messages, restart WiFi AP |

---

## Maintenance

### Daily Checks (Automated)

- Monitor uptime (should be continuous)
- Check CRC error rate (<1%)
- Verify device count matches expected

### Weekly Checks

- [ ] Review event log for anomalies
- [ ] Check for firmware updates
- [ ] Verify all devices still online
- [ ] Test WiFi AP functionality

### Monthly Checks

- [ ] Clean dust from enclosure (if used)
- [ ] Check screw terminal tightness
- [ ] Verify RS-485 cable condition
- [ ] Test backup/restore configuration

### Firmware Updates

1. **Backup configuration:**
   ```
   GET_CONFIG
   # Save output to file
   ```

2. **Download new firmware** from repository

3. **Upload via PlatformIO:**
   ```bash
   pio run --target upload
   ```

4. **Restore configuration:**
   ```
   SET_BAUD,9600
   # etc.
   ```

5. **Verify operation** with bench test

### Backup Procedures

**Configuration backup:**
```
GET_CONFIG > config_backup.txt
```

**Device list backup:**
```
SCAN > devices_backup.txt
```

**Event log backup:**
- Enable logging to SD card (if added)
- Or capture serial output to file

---

## Safety & Compliance

### Electrical Safety

- ⚠️ **Never work on live circuits** — Power off Orion panel before wiring
- ⚠️ **Use isolated RS-485** — ADM2483 provides 2.5kV protection
- ⚠️ **Check polarity** — Reversing A/B won't damage ADM2483 but won't work
- ⚠️ **Proper grounding** — Do NOT connect ADM2483 GND_ISO to ESP32 GND

### Site Safety

- **Label all cables** clearly
- **Secure gateway** in enclosure (optional but recommended)
- **Keep away from moisture** — IP54 rated enclosure if needed
- **Ventilation** — Ensure airflow for heat dissipation
- **Access control** — Restrict physical access to authorized personnel

### Regulatory Compliance

**If deploying commercially:**
- CE marking (Europe)
- FCC Part 15 (USA)
- RoHS compliance (lead-free solder)
- Local electrical codes

---

## Support & Resources

### Documentation

- `docs/esp32c3_schematic_final.md` — Circuit diagram and pinout
- `docs/BOM_complete.md` — Component list
- `firmware/esp32_orion_slave/README.md` — Firmware documentation

### Datasheets

- ESP32-C3: https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
- ADM2483: https://www.analog.com/media/en/technical-documentation/data-sheets/ADM2483.pdf
- AMS1117: http://www.advanced-monolithic.com/pdf/ds1117.pdf

### Community

- GitHub Issues: Report bugs and request features
- Forum: Discuss integration challenges
- Email: Technical support contact

---

## Appendix A: Quick Reference

### Serial Commands

| Command | Description | Example |
|---------|-------------|---------|
| `GET_STATUS` | System status | `GET_STATUS` |
| `GET_CONFIG` | Current config | `GET_CONFIG` |
| `SET_BAUD,<rate>` | Change baud rate | `SET_BAUD,9600` |
| `SCAN` | Scan for devices | `SCAN` |
| `MASTER_ON` | Enable MASTER mode | `MASTER_ON` |
| `MASTER_OFF` | Disable MASTER mode | `MASTER_OFF` |
| `PING` | Test connectivity | `PING` |

### LED Indicators

| LED | Color | Pattern | Meaning |
|-----|-------|---------|---------|
| Power | Yellow | Solid ON | 3.3V_EXT OK |
| Status | Green | Slow blink | Normal operation |
| Status | Green | Fast blink | High bus activity |
| Debug | Red | OFF | Debug mode off |
| Debug | Red | Fast blink | Debug mode active |
| Debug | Red | Slow blink | WiFi AP active |

### GPIO Pin Reference

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| GPIO2 | RS-485 DE/RE | Output | LOW = receive only |
| GPIO3 | Status LED | Output | Via Q1 transistor |
| GPIO4 | Debug LED | Output | Via Q2 transistor |
| GPIO5 | Debug button | Input | Active LOW, pull-up |
| GPIO6 | RS-485 RX | Input | From ADM2483 RO |
| GPIO7 | RS-485 TX | Output | To ADM2483 DI |
| GPIO8 | Mode switch | Input | LOW=MONITOR, HIGH=MASTER |
| GPIO20 | External RX | Input | UART0 (shared with USB) |
| GPIO21 | External TX | Output | UART0 (shared with USB) |

---

## Appendix B: Wiring Diagrams

### Minimal Wiring (USB + RS-485 Only)

```
PC USB-C ──────► ESP32-C3 USB-C
                     │
                     ├─ 5V pin ──► AMS1117 VIN
                     │                │
                     │           AMS1117 VOUT ──► 3.3V_EXT
                     │                               │
                     │                               ├──► ADM2483 VCC
                     │                               └──► LEDs, pull-ups
                     │
                     ├─ GPIO6 ◄──── ADM2483 RO
                     ├─ GPIO7 ────► ADM2483 DI
                     ├─ GPIO2 ────► ADM2483 DE+RE
                     │
                     └─ GND ──────── Common GND
                                         │
                                    ADM2483 GND

Orion Panel RS-485:
    A ──────────────────────► ADM2483 A (isolated)
    B ──────────────────────► ADM2483 B (isolated)
    GND (do NOT connect) ─ ✗ ─ ADM2483 GND_ISO (leave floating)
```

### Full Wiring (All Features)

See `docs/esp32c3_schematic_final.md` for complete circuit diagram.

---

**End of Installation Guide**

For technical support, refer to project documentation or contact the development team.
