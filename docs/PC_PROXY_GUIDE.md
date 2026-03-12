# Orion RS-485 PC Proxy — Complete Guide

> **One application** that connects your PC to the Bolid Orion RS-485 security bus,
> decodes all protocol traffic, and serves a real-time web dashboard + TCP API
> for client applications.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Prerequisites](#2-prerequisites)
3. [Hardware Setup](#3-hardware-setup)
4. [Installation](#4-installation)
5. [First Run — Quick Start](#5-first-run--quick-start)
6. [Run Modes](#6-run-modes)
7. [Configuration](#7-configuration)
8. [Web Dashboard](#8-web-dashboard)
9. [Connecting Client Applications (TCP API)](#9-connecting-client-applications-tcp-api)
10. [TCP Command Reference](#10-tcp-command-reference)
11. [Supported Devices](#11-supported-devices)
12. [Troubleshooting](#12-troubleshooting)
13. [Architecture](#13-architecture)
14. [File Structure](#14-file-structure)

---

## 1. Overview

```
 ┌──────────────┐     RS-485      ┌──────────────────────────────────────┐
 │  C2000M      │────── bus ──────│  PC  (orion_proxy.exe)               │
 │  Signal-20M  │                 │                                      │
 │  Other Bolid │                 │  ┌──────────┐  ┌──────────────────┐  │
 │  devices     │                 │  │ RS-485   │  │ Device Tracker   │  │
 └──────────────┘                 │  │ Sniffer  │──│ Events / Status  │  │
                                  │  └──────────┘  └────────┬─────────┘  │
                                  │                         │            │
              USB-RS485           │  ┌──────────────────────┼─────────┐  │
              adapter             │  │  TCP Server :9100    │         │  │
                                  │  │  (client apps)      │         │  │
                                  │  ├──────────────────────┤         │  │
                                  │  │  HTTP Dashboard :8080│         │  │
                                  │  │  (web browser)       │         │  │
                                  │  └──────────────────────┘         │  │
                                  │                                      │
                                  │  ┌────────────────────────────────┐  │
                                  │  │  System Tray Icon (--tray)    │  │
                                  │  └────────────────────────────────┘  │
                                  └──────────────────────────────────────┘
```

**What the proxy provides:**

- **Passive monitoring** — Decode all RS-485 traffic without interfering
- **Active commands** — Arm, disarm, reset, scan devices (MASTER mode)
- **Real-time web dashboard** — Status, devices, events, config in your browser
- **TCP API** — Any app can connect and receive data or send commands
- **System tray mode** — Runs silently with a tray icon (Windows)
- **Background mode** — Runs as a headless service

---

## 2. Prerequisites

### Hardware

| Item | Purpose | Approximate Cost |
|------|---------|-----------------|
| USB-RS485 adapter | Connect PC to RS-485 bus | $3 – $20 |
| 2-wire cable | A/B data lines | From your existing bus |

**Recommended adapters** (isolated for safety):
- **FTDI USB-RS485** (isolated) — Best reliability
- **CH340 USB-RS485** — Budget option, works well
- **Waveshare USB TO RS485** — Industrial grade

> See `docs/pc_rs485_hardware.md` for detailed wiring diagrams and isolation options.

### Software

| Requirement | Notes |
|-------------|-------|
| Windows XP SP3+ or Linux | XP, 7, 8, 10, 11 all supported |
| USB driver | CH340 or FTDI driver for your adapter |
| Web browser | For the dashboard (Chrome, Firefox, Edge, IE9+) |

### Windows Version Compatibility

| Windows Version | Status | Notes |
|----------------|--------|-------|
| **Windows XP SP3** | ✅ Compatible | Requires Visual C++ Runtime or use MinGW build |
| **Windows 7/8/8.1** | ✅ Fully supported | No issues |
| **Windows 10/11** | ✅ Fully supported | Tested and recommended |
| **Windows Server** | ✅ Compatible | 2003 R2+ supported |

The application uses standard Win32 API and Winsock2 (available since Windows 95), so it works on older systems. The current GCC build is statically linked and requires no additional runtime libraries.

---

## 3. Hardware Setup

### Step 1: Install USB-RS485 Driver

1. Plug in your USB-RS485 adapter
2. Windows should auto-install drivers. If not:
   - CH340: Download from [wch-ic.com](http://www.wch-ic.com/downloads/CH341SER_EXE.html)
   - FTDI: Download from [ftdichip.com](https://ftdichip.com/drivers/)
3. Open **Device Manager** → **Ports (COM & LPT)**
4. Note the COM port number (e.g., `COM5`)

### Step 2: Connect to RS-485 Bus

```
USB-RS485 Adapter          Orion RS-485 Bus
┌─────────────┐            ┌─────────────────┐
│  A (Data+)  │────────────│  A (Data+)      │
│  B (Data-)  │────────────│  B (Data-)      │
│  GND        │────────────│  GND (optional) │
└─────────────┘            └─────────────────┘
```

**Important:**
- Connect **A to A** and **B to B** — do NOT swap
- GND connection recommended for long cable runs (>10m)
- If adapter is at the end of the bus, enable 120Ω termination
- Maximum bus length: 1200m at 9600 baud

### Step 3: Verify Connection

The adapter's TX/RX LEDs should blink when devices communicate on the bus.
If using an isolated adapter, no additional isolation is needed.

---

## 4. Installation

### Windows Installation

1. **Copy files** to your desired location:
   ```
   C:\OrionProxy\
   ├── orion_proxy.exe        ← Main executable
   └── dashboard.html         ← Web dashboard (must be next to exe)
   ```

2. **(Optional) Generate a config file:**
   ```cmd
   orion_proxy.exe -p COM5 --save-config
   ```
   This creates `orion_proxy.ini` with your settings.

3. **(Optional) Create a desktop shortcut for tray mode:**
   ```
   Target:    C:\OrionProxy\orion_proxy.exe -p COM5 --tray
   Start in:  C:\OrionProxy\
   ```

4. **(Optional) Auto-start with Windows:**
   - Press `Win+R`, type `shell:startup`
   - Place a shortcut to `orion_proxy.exe --tray -p COM5` in that folder

### Linux Installation

**Quick build:**
```bash
cd pc/orion_proxy
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
./orion_proxy -p /dev/ttyUSB0
```

**Detailed instructions:** See `docs/LINUX_BUILD_GUIDE.md` for:
- Dependencies for Ubuntu, CentOS, Fedora, Arch, Raspberry Pi
- Cross-compilation for ARM
- systemd service setup
- Packaging as .deb or .rpm
- Troubleshooting serial port permissions

---

## 5. First Run — Quick Start

### Simplest: Console mode

```cmd
orion_proxy.exe -p COM5
```

You will see:
```
╔══════════════════════════════════════════╗
║   Orion RS-485 Proxy Service v1.0        ║
╠══════════════════════════════════════════╣
║  COM port:  COM5                         ║
║  Baud rate: 9600                         ║
║  TCP port:  9100                         ║
║  Dashboard: http://localhost:8080        ║
║  Mode:      MONITOR (passive)            ║
║  Debug:     OFF                          ║
║  Key:       0x00                         ║
╚══════════════════════════════════════════╝

[HTTP] Dashboard at http://localhost:8080
[MAIN] Sniffer thread started (MONITOR mode)
[MAIN] Ready. Press Ctrl+C to stop.
```

**Open your browser** to `http://localhost:8080` — you will see the live dashboard.

### Even without RS-485 hardware

The proxy runs in **dashboard-only mode** if no serial port is available:

```cmd
orion_proxy.exe
```

The dashboard will be accessible, showing "0 devices". This lets you explore the UI and configure settings before connecting hardware.

---

## 6. Run Modes

### Console Mode (default)

```cmd
orion_proxy.exe -p COM5 -v
```

Shows a terminal window with live output. Best for debugging.

### System Tray Mode (`--tray`)

```cmd
orion_proxy.exe -p COM5 --tray
```

- Hides the console window
- Shows a **system tray icon** (notification area)
- **Double-click** tray icon → opens dashboard in browser
- **Right-click** tray icon → menu:
  - Open Dashboard
  - Show Console Log
  - Exit
- Tooltip shows live device count and packet stats
- Logging automatically enabled to `orion_proxy.log`

### Background Mode (`--background`)

```cmd
orion_proxy.exe -p COM5 --background
```

- No console window
- No tray icon
- Runs completely headless
- All output goes to log file
- Dashboard still accessible at `http://localhost:8080`
- Stop by killing the process or via TCP `QUIT` command

---

## 7. Configuration

### Configuration Sources (priority order)

1. **Config file** (`orion_proxy.ini`) — loaded on startup
2. **Command-line arguments** — override config file
3. **Runtime changes** — via web dashboard or TCP commands

### Config File Format

Create with `--save-config` or write manually:

```ini
# orion_proxy.ini
[serial]
com_port = COM5
baud_rate = 9600
serial_timeout_ms = 100

[server]
tcp_port = 9100
http_port = 8080

[protocol]
encryption_key = 0
device_timeout_sec = 30
scan_interval_sec = 0

[mode]
master_mode = 0
auto_scan = 0

[debug]
debug_level = 0
log_to_file = 0
log_file = orion_proxy.log
verbose = 0
show_timestamps = 1
```

### Command-Line Options

| Flag | Description | Default |
|------|-------------|---------|
| `-p <port>` | COM port | COM3 (Win) / /dev/ttyUSB0 (Linux) |
| `-b <baud>` | Baud rate | 9600 |
| `-t <port>` | TCP server port | 9100 |
| `-w <port>` | Web dashboard port | 8080 |
| `-k <key>` | Encryption key (0-255) | 0 |
| `-m` | MASTER mode (active commands) | MONITOR |
| `-v` | Verbose console output | Off |
| `-d <0-3>` | Debug level | 0 (off) |
| `--auto-scan` | Scan all devices on start | Off |
| `--log <file>` | Enable file logging | Off |
| `-c <file>` | Config file path | orion_proxy.ini |
| `--tray` | System tray mode | Off |
| `--background` | Headless background mode | Off |
| `--save-config` | Save config and exit | — |
| `-h, --help` | Show help | — |

### All Settings

| Setting | Values | Description |
|---------|--------|-------------|
| `com_port` | COM1-COM99, /dev/ttyUSB* | Serial port name |
| `baud_rate` | 9600, 19200, 38400, 57600 | Orion standard is 9600 |
| `serial_timeout_ms` | 50-500 | Serial read timeout |
| `tcp_port` | 1-65535 | TCP API server port |
| `http_port` | 1-65535 | Web dashboard port |
| `encryption_key` | 0-255 | Orion bus encryption key (0 = none) |
| `device_timeout_sec` | 5-300 | Seconds before device goes offline |
| `scan_interval_sec` | 0-3600 | Auto-scan interval (0 = disabled) |
| `master_mode` | 0/1 | 0 = passive monitor, 1 = active master |
| `auto_scan` | 0/1 | Scan all addresses on startup |
| `debug_level` | 0-3 | 0=off, 1=events, 2=packets, 3=hex dump |
| `log_to_file` | 0/1 | Enable file logging |
| `log_file` | filename | Log file path |
| `verbose` | 0/1 | Verbose console stats |
| `show_timestamps` | 0/1 | Timestamp prefix on log lines |

### Runtime Configuration (via Dashboard)

1. Open `http://localhost:8080`
2. Go to **Configuration** tab
3. Change settings
4. Click **Apply All Changes**
5. Click **Save to File** to persist across restarts

---

## 8. Web Dashboard

The dashboard is built into the proxy — no separate application needed.

### Access

Open `http://localhost:8080` in any browser.
(Port is configurable with `-w` flag.)

### Dashboard Tabs

| Tab | Description |
|-----|-------------|
| **Overview** | Stats cards (devices, packets, errors, uptime), online devices table, recent events |
| **Devices** | Full device table with address, model, status, packet count. Scan bus button. Query individual devices. |
| **Live Events** | Real-time event stream with auto-scroll, color-coded by type |
| **Configuration** | Edit all proxy settings. Apply instantly or save to file. |
| **Debug** | Set debug level (0-3) with one click. Quick action buttons. |
| **Console** | Raw TCP command console — type any command and see response |

### Auto-Refresh

- **Status**: Every 3 seconds
- **Events**: Every 1.5 seconds
- **Devices**: Every 10 seconds

---

## 9. Connecting Client Applications (TCP API)

Any application can connect to the proxy's TCP server to receive data and send commands.

### Connection

```
Host: localhost (or the PC's IP address for remote access)
Port: 9100 (configurable)
Protocol: Plain text, line-based (\n terminated)
```

### Quick Test with Telnet

```cmd
telnet localhost 9100
```

Then type:
```
HELP
GET_STATUS
GET_DEVICES
```

### Client Connection Flow

```
1. Connect TCP socket to localhost:9100
2. Receive welcome message: "ORION_PROXY v1.0\n"
3. Send commands, receive responses
4. (Optional) SUBSCRIBE_EVENTS for real-time push
5. QUIT when done
```

### Python Client Example

```python
import socket

def orion_client():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 9100))

    # Read welcome
    print(sock.recv(1024).decode())

    # Get status
    sock.sendall(b'GET_STATUS\n')
    print(sock.recv(4096).decode())

    # Get devices
    sock.sendall(b'GET_DEVICES\n')
    print(sock.recv(4096).decode())

    # Subscribe to events
    sock.sendall(b'SUBSCRIBE_EVENTS\n')
    print(sock.recv(1024).decode())

    # Receive events in real-time
    while True:
        data = sock.recv(4096)
        if not data:
            break
        print(data.decode(), end='')

orion_client()
```

### C# Client Example

```csharp
using System.Net.Sockets;
using System.Text;

var client = new TcpClient("localhost", 9100);
var stream = client.GetStream();
var buffer = new byte[4096];

// Read welcome
int n = stream.Read(buffer, 0, buffer.Length);
Console.WriteLine(Encoding.ASCII.GetString(buffer, 0, n));

// Send command
byte[] cmd = Encoding.ASCII.GetBytes("GET_STATUS\n");
stream.Write(cmd, 0, cmd.Length);

n = stream.Read(buffer, 0, buffer.Length);
Console.WriteLine(Encoding.ASCII.GetString(buffer, 0, n));
```

### Node.js Client Example

```javascript
const net = require('net');
const client = net.createConnection({ port: 9100 }, () => {
  client.write('GET_STATUS\n');
});
client.on('data', (data) => {
  console.log(data.toString());
  client.write('SUBSCRIBE_EVENTS\n');
});
```

### REST API (HTTP)

Client apps can also use the HTTP JSON API:

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Proxy status (JSON) |
| GET | `/api/devices` | All devices (JSON array) |
| GET | `/api/events` | Recent events (JSON array) |
| GET | `/api/config` | Current config (JSON) |
| GET | `/api/live_events` | Buffered live events |
| GET | `/api/device_types` | Known Bolid device types |
| GET | `/api/ping` | Health check |
| POST | `/api/command` | Send raw command `{"command":"..."}` |
| POST | `/api/set` | Set config `{"key":"...","value":"..."}` |
| POST | `/api/debug_level` | Set debug `{"level":0}` |
| POST | `/api/save_config` | Save config to file |
| POST | `/api/scan_device` | Query device `{"address":1}` |

**Example with curl:**

```bash
# Get status
curl http://localhost:8080/api/status

# Get all devices
curl http://localhost:8080/api/devices

# Set debug level
curl -X POST http://localhost:8080/api/debug_level -d '{"level":2}'

# Query device at address 5
curl -X POST http://localhost:8080/api/scan_device -d '{"address":5}'
```

---

## 10. TCP Command Reference

### Information Commands

| Command | Response | Description |
|---------|----------|-------------|
| `PING` | `PONG` | Connection health check |
| `HELP` | Command list | Show all available commands |
| `GET_STATUS` | Status line | Packets, errors, devices, uptime |
| `GET_DEVICES` | Device list | All tracked devices with status |
| `GET_EVENTS,<count>` | Event list | Last N events |
| `DEVICE_TYPES` | Type table | All known Bolid device codes |

### Event Subscription

| Command | Description |
|---------|-------------|
| `SUBSCRIBE_EVENTS` | Receive event push notifications |
| `UNSUBSCRIBE_EVENTS` | Stop event notifications |
| `SUBSCRIBE_RAW` | Receive raw hex packet data |
| `UNSUBSCRIBE_RAW` | Stop raw data |

### Configuration Commands

| Command | Description |
|---------|-------------|
| `GET_CONFIG` | Show all configuration settings |
| `SET,<key>=<value>` | Change a setting at runtime |
| `SAVE_CONFIG` | Write config to file |

### Debug Commands

| Command | Description |
|---------|-------------|
| `DEBUG` | Show current debug level |
| `DEBUG,<0-3>` | Set debug level |

### Active Commands (MASTER mode only)

| Command | Description |
|---------|-------------|
| `SCAN` | Scan all addresses (1-127) |
| `SCAN_DEVICE,<addr>` | Query single device info |
| `READ_STATUS,<addr>` | Read device status |
| `ARM,<addr>,<zone>` | Arm a zone |
| `DISARM,<addr>,<zone>` | Disarm a zone |
| `RESET_ALARM,<addr>` | Reset alarm for device |

### Session Commands

| Command | Description |
|---------|-------------|
| `QUIT` | Disconnect gracefully |

### Response Format

```
STATUS,uptime=123,packets=4567,crc_errors=0,error_rate=0.00%,...
DEVICE,1,ONLINE,type=0x06,model=Signal-20M,packets=234,...
EVENT,id=15,device=3,code=0x81,zone=1,...
OK,<message>
ERROR,<message>
END
```

---

## 11. Supported Devices

The proxy auto-identifies these Bolid Orion devices:

| Code | Model | Category | Zones |
|------|-------|----------|-------|
| 0x01 | C2000M | Controller | 0 |
| 0x06 | Signal-20M | Fire/Security Panel | 20 |
| 0x04 | Signal-20 | Fire Panel | 20 |
| 0x05 | Signal-20P | Fire Panel | 20 |
| 0x10 | S2000-4 | Access Controller | 4 |
| 0x0A | S2000-KDL | Addressable Loop | 127 |
| 0x0E | S2000-SP1 | Siren Controller | 1 |
| 0x16 | S2000-2 | Security Panel | 2 |
| 0x19 | C2000-GSM | GSM Module | 0 |
| 0x1A | C2000-Ethernet | Network Module | 0 |
| 0x1D | Signal-10 | Security Panel | 10 |
| 0x37 | C2000M v2 | Controller | 0 |

And more — use the `DEVICE_TYPES` command or check the **Devices** tab in the dashboard for the full list.

---

## 12. Troubleshooting

### Serial port won't open

| Symptom | Fix |
|---------|-----|
| "Failed to open serial port" | Check COM port number in Device Manager |
| "Access denied" | Close other programs using the port (Uprog, PProg) |
| Adapter not listed | Install CH340 or FTDI driver |
| Wrong port number | Try each COM port or use `mode` command |

**Find your COM port (Windows):**
```cmd
mode
```
Or: Device Manager → Ports (COM & LPT)

**Find your port (Linux):**
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# If permission denied:
sudo usermod -aG dialout $USER
# Then log out and back in
```

### No devices shown

| Symptom | Fix |
|---------|-----|
| 0 devices, 0 packets | Check A/B wiring, verify bus is active |
| Packets but no devices | Check encryption key (`-k`) matches the bus |
| Devices appear then go offline | Increase timeout: `-d 30` or dashboard config |

### Dashboard not loading

| Symptom | Fix |
|---------|-----|
| Page not found | Ensure `dashboard.html` is next to `orion_proxy.exe` |
| Cannot connect | Check port 8080 isn't blocked by firewall |
| Blank page | Try `http://127.0.0.1:8080` instead of localhost |

### Firewall

If accessing from another PC on the network:
```cmd
netsh advfirewall firewall add rule name="Orion Proxy HTTP" dir=in action=allow protocol=TCP localport=8080
netsh advfirewall firewall add rule name="Orion Proxy TCP" dir=in action=allow protocol=TCP localport=9100
```

### Testing Without Hardware

If you don't have RS-485 hardware or can't visit the site yet:

**Option 1: Virtual COM Port + Simulator**
- Install com0com (free virtual COM port driver)
- Use the included Python simulator to send fake Orion packets
- Full instructions: `docs/TESTING_WITHOUT_HARDWARE.md`

**Option 2: Dashboard-Only Mode**
```cmd
orion_proxy.exe -p COM99
# Dashboard works at http://localhost:8080 even without serial port
```

This lets you test the dashboard, configuration, and client APIs before deploying to the real site.

---

## 13. Architecture

### Components

```
orion_proxy.exe
├── Serial Sniffer Thread
│   └── Reads RS-485 packets, decrypts, parses Orion protocol
├── Device Tracker
│   ├── Maintains online/offline state for each address (1-127)
│   ├── Event ring buffer (256 entries)
│   └── Status tracking per device
├── TCP Server Thread (port 9100)
│   ├── Up to 16 simultaneous client connections
│   ├── Text-based command protocol
│   └── Event push to subscribed clients
├── HTTP Server Thread (port 8080)
│   ├── Serves dashboard.html
│   ├── REST JSON API endpoints
│   └── Live event buffer for polling
└── System Tray (optional, Windows)
    ├── Win32 notification area icon
    ├── Right-click context menu
    └── Tooltip with live stats
```

### Threading Model

| Thread | Purpose |
|--------|---------|
| **Main thread** | Config, init, main loop (TCP poll, timeouts) |
| **Sniffer thread** | Blocking serial read, packet decode |
| **HTTP thread** | Accept + serve HTTP connections |
| **Tray thread** | Win32 message loop (optional) |

### Data Flow

```
RS-485 Bus → Serial Port → Sniffer → Device Tracker → TCP Server → Clients
                                          │            HTTP Server → Dashboard
                                          └─→ Event Buffer
```

---

## 14. File Structure

```
pc/orion_proxy/
├── build/
│   ├── orion_proxy.exe          ← Compiled executable
│   └── dashboard.html           ← Web dashboard (copy here)
├── gui/
│   └── dashboard.html           ← Dashboard source
├── include/
│   ├── device_tracker.h         ← Device state tracking
│   ├── proxy_server.h           ← TCP server
│   ├── proxy_config.h           ← Configuration system
│   ├── orion_device_types.h     ← Bolid device identification
│   ├── http_server.h            ← Embedded HTTP server
│   └── tray_icon.h              ← Windows system tray
├── src/
│   ├── main.c                   ← Entry point, orchestration
│   ├── device_tracker.c         ← Device tracking logic
│   ├── proxy_server.c           ← TCP server + commands
│   ├── proxy_config.c           ← Config load/save/set
│   ├── orion_device_types.c     ← Device type database
│   ├── http_server.c            ← HTTP server + REST API
│   └── tray_icon.c              ← Win32 tray icon
├── CMakeLists.txt               ← Build configuration
└── README.md                    ← Quick reference
```

### Deployment (Minimum Files)

```
C:\OrionProxy\
├── orion_proxy.exe
├── dashboard.html
└── orion_proxy.ini   (auto-generated with --save-config)
```

---

## Quick Reference Card

```
START:          orion_proxy.exe -p COM5
TRAY MODE:      orion_proxy.exe -p COM5 --tray
BACKGROUND:     orion_proxy.exe -p COM5 --background
DASHBOARD:      http://localhost:8080
TCP API:        telnet localhost 9100
SAVE CONFIG:    orion_proxy.exe -p COM5 --save-config
MASTER MODE:    orion_proxy.exe -p COM5 -m --auto-scan
VERBOSE:        orion_proxy.exe -p COM5 -v -d 2
```
