# Orion RS-485 Proxy Service

## Architecture

```
┌──────────────┐     ┌─────────────────────────────────────────────┐
│  USB-RS485   │     │           Orion Proxy Service               │
│  Adapter     │◄───►│                                             │
│  (COM port)  │     │  ┌─────────┐  ┌──────────┐  ┌───────────┐ │
└──────────────┘     │  │ Sniffer │  │ Device   │  │ TCP       │ │
       │             │  │ Thread  │─►│ Tracker  │◄─│ Server    │ │
       │             │  └─────────┘  └──────────┘  └─────┬─────┘ │
   RS-485 Bus        │                                    │       │
       │             └────────────────────────────────────┼───────┘
       ▼                                                  │
┌──────────────┐                                   TCP port 9100
│ Orion Panel  │                                          │
│ C2000M +     │                          ┌───────────────┼───────────────┐
│ all devices  │                          │               │               │
└──────────────┘                     ┌────┴────┐   ┌──────┴──────┐  ┌─────┴─────┐
                                     │Client 1 │   │ Client 2    │  │ Client N  │
                                     │Dashboard│   │ Event Logger│  │ Alarm Mgr │
                                     └─────────┘   └─────────────┘  └───────────┘
```

## How It Works

1. **Sniffer thread** reads all RS-485 traffic via USB adapter (passive, never transmits in MONITOR mode)
2. **Device tracker** decodes Orion protocol (CRC, encryption, packet framing) and tracks all 127 devices
3. **TCP server** accepts multiple client connections on port 9100
4. **Clients** receive real-time events and can query device status or send commands

## Requirements

- **USB-RS485 adapter** (isolated recommended — see `docs/INSTALLATION_GUIDE.md`)
- **CMake 3.14+**
- **C compiler** (MSVC, GCC, or Clang)
- Windows or Linux

## Building

### Windows (MSVC)

```batch
cd pc\orion_proxy
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Windows (MinGW)

```batch
cd pc/orion_proxy
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Linux

```bash
cd pc/orion_proxy
mkdir build && cd build
cmake ..
make
```

## Usage

### Start the Proxy

```bash
# Windows
orion_proxy.exe -p COM5 -b 9600 -t 9100

# Linux
./orion_proxy -p /dev/ttyUSB0 -b 9600 -t 9100
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-p <port>` | COM port name | `COM3` / `/dev/ttyUSB0` |
| `-b <baud>` | Baud rate | `9600` |
| `-t <port>` | TCP listen port | `9100` |
| `-k <key>` | Orion encryption key (0-255) | `0` |
| `-m` | Start in MASTER mode (can send commands to bus) | MONITOR |
| `-d <level>` | Debug level: 0=off, 1=events, 2=packets, 3=hex | `0` |
| `-v` | Verbose console output | Off |
| `--auto-scan` | Scan all 127 addresses on startup (needs `-m`) | Off |
| `--log <file>` | Enable logging to file | Off |
| `-c <file>` | Load config from INI file | `orion_proxy.ini` |
| `--save-config` | Generate config file and exit | — |
| `-h` | Show help | — |

### Configuration File

The proxy auto-loads `orion_proxy.ini` on startup. Generate a default config:

```bash
orion_proxy.exe --save-config
```

Example `orion_proxy.ini`:
```ini
[serial]
com_port = COM5
baud_rate = 9600
serial_timeout_ms = 100

[server]
tcp_port = 9100

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

**Config priority:** INI file → command-line args → runtime TCP commands.
All settings can be changed at runtime via TCP `SET,key=value` command.

### Connect a Client

```bash
# Using telnet
telnet localhost 9100

# Using netcat
nc localhost 9100

# Using any TCP socket library in your language
```

## Supported Bolid Devices

The proxy automatically identifies these device types on the RS-485 bus:

| Type Code | Model | Category | Zones |
|-----------|-------|----------|-------|
| `0x01` | **C2000M** | Panel/Controller | — |
| `0x06` | **Signal-20M** | Zone Expander | 20 |
| `0x34` | **Signal-20M (v2)** | Zone Expander | 20 |
| `0x37` | **C2000M (v2)** | Panel/Controller | — |
| `0x04` | Signal-20 | Zone Expander | 20 |
| `0x0A` | S2000-KDL | Loop Controller | — |
| `0x0E` | S2000-SP1 | Relay Output | 1 |
| `0x10` | S2000-4 | Zone Expander | 4 |
| `0x16` | S2000-2 | Zone Expander | 2 |
| `0x1D` | Signal-10 | Zone Expander | 10 |
| `0x19` | C2000-GSM | Communicator | — |
| `0x1A` | C2000-Ethernet | Communicator | — |

Full list available via `DEVICE_TYPES` TCP command.

## Client Protocol

Text-based, line-oriented (each command/response ends with `\n`).

### Query Commands

| Command | Response |
|---------|----------|
| `PING` | `PONG` |
| `GET_STATUS` | System status (packets, errors, uptime, online count) |
| `GET_DEVICES` | All known devices + `END` |
| `GET_DEVICE,<addr>` | Single device details |
| `GET_EVENTS[,<count>]` | Recent events + `END` |
| `DEVICE_TYPES` | All known Bolid device type codes |
| `HELP` | Full command list |
| `QUIT` | `BYE` (disconnect) |

### Subscription Commands

| Command | Description |
|---------|-------------|
| `SUBSCRIBE_EVENTS` | Receive real-time event broadcasts (auto-enabled) |
| `UNSUBSCRIBE_EVENTS` | Stop receiving events |
| `SUBSCRIBE_RAW` | Receive raw packet hex dumps |
| `UNSUBSCRIBE_RAW` | Stop receiving raw packets |

### MASTER Mode Commands (active bus communication)

| Command | Description |
|---------|-------------|
| `SCAN` | Scan all 127 addresses (broadcasts `DEVICE_FOUND` events) |
| `SCAN_DEVICE,<addr>` | Query single device type, firmware, zones |
| `READ_STATUS,<addr>` | Read live status from device |
| `ARM,<addr>,<zone>` | Arm a zone |
| `DISARM,<addr>,<zone>` | Disarm a zone |
| `RESET_ALARM,<addr>,<zone>` | Reset alarm |

### Configuration Commands (runtime)

| Command | Description |
|---------|-------------|
| `GET_CONFIG` | Show all current settings |
| `SET,<key>=<value>` | Change a setting (e.g., `SET,debug_level=2`) |
| `SAVE_CONFIG` | Persist settings to INI file |

### Debug Commands (runtime)

| Command | Description |
|---------|-------------|
| `DEBUG` | Show current debug status |
| `DEBUG,0` | Debug OFF |
| `DEBUG,1` | Show events only |
| `DEBUG,2` | Show events + packet summaries |
| `DEBUG,3` | Show events + packets + full hex dumps |

### Response Formats

```
# Status
STATUS,uptime=3600,packets=1523,crc_errors=2,error_rate=0.13%,devices_online=3,events=45

# Device list (enriched with model name)
DEVICE,1,ONLINE,type=0x01,status1=0x04,status2=0x00,packets=523,last_seen=1710234567
DEVICE,5,ONLINE,type=0x06,status1=0x06,status2=0x00,packets=312,last_seen=1710234560
END

# Device scan result
DEVICE_INFO,1,model=C2000M,type=0x01,category=Panel/Controller,fw=3.12,zones=0,name=...
DEVICE_INFO,5,model=Signal-20M,type=0x06,category=Zone Expander,fw=2.04,zones=20,name=...

# Real-time event (pushed to subscribed clients)
PACKET,dev=1,model=C2000M,len=8,enc=1,status=0x04(Armed)

# Raw packet (pushed to raw subscribers)
RAW,addr=1,len=8,enc=1,hex=810857020400C7A3

# Config
CONFIG:
  com_port = COM5
  baud_rate = 9600
  master_mode = 1 (MASTER)
  debug_level = 2 (PACKETS)
  ...
END
```

## Integration Examples

### Python Client

```python
import socket

def connect_proxy(host='localhost', port=9100):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    # Read welcome message
    welcome = sock.recv(4096).decode()
    print(welcome)
    return sock

def send_command(sock, cmd):
    sock.sendall((cmd + '\n').encode())
    response = sock.recv(4096).decode()
    return response

# Usage
sock = connect_proxy()
print(send_command(sock, 'GET_STATUS'))
print(send_command(sock, 'GET_DEVICES'))
print(send_command(sock, 'SUBSCRIBE_EVENTS'))

# Listen for real-time events
while True:
    data = sock.recv(4096).decode()
    if data:
        print(f"Event: {data.strip()}")
```

### C# Client

```csharp
using System.Net.Sockets;
using System.IO;

var client = new TcpClient("localhost", 9100);
var stream = client.GetStream();
var reader = new StreamReader(stream);
var writer = new StreamWriter(stream) { AutoFlush = true };

// Read welcome
Console.WriteLine(reader.ReadLine());

// Query status
writer.WriteLine("GET_STATUS");
Console.WriteLine(reader.ReadLine());

// Subscribe to events
writer.WriteLine("SUBSCRIBE_EVENTS");
while (true) {
    string line = reader.ReadLine();
    Console.WriteLine($"Event: {line}");
}
```

### Node.js Client

```javascript
const net = require('net');

const client = net.createConnection({ port: 9100 }, () => {
    client.write('SUBSCRIBE_EVENTS\n');
});

client.on('data', (data) => {
    const lines = data.toString().split('\n');
    lines.forEach(line => {
        if (line.startsWith('PACKET,') || line.startsWith('EVENT,')) {
            console.log('Event:', line);
        }
    });
});
```

## Hardware

See `docs/pc_rs485_hardware.md` for detailed hardware connection options:
- **Option 1:** Buy an isolated USB-RS485 adapter ($10-20, recommended)
- **Option 2:** Build a custom isolation board (ADUM1201+MAX485+B0505S)
- **Option 3:** USB isolator module + standard USB-RS485 adapter

**Yes, the proxy works identically to the ESP32 gateway** — same protocol, same
devices (C2000M, Signal-20M, etc.), same functionality. The proxy just runs on
the PC instead of custom hardware.

## File Structure

```
pc/orion_proxy/
├── CMakeLists.txt            — Build configuration
├── README.md                 — This file
├── include/
│   ├── device_tracker.h      — Device state tracking (127 devices)
│   ├── proxy_server.h        — TCP server for multi-client access
│   ├── proxy_config.h        — Configuration system (INI file + runtime)
│   └── orion_device_types.h  — Bolid device type database (C2000M, Signal-20M, etc.)
└── src/
    ├── main.c                — Entry point, sniffer thread, scan, logging
    ├── device_tracker.c      — Device state management
    ├── proxy_server.c        — TCP server + command processing
    ├── proxy_config.c        — INI file parser, runtime config
    └── orion_device_types.c  — Device names, categories, zone counts
```

**Depends on parent library:**
```
pc/
├── include/
│   ├── orion_crc.h         — CRC-8-Dallas
│   ├── orion_serial.h      — Serial port abstraction
│   ├── orion_protocol.h    — Packet build/parse/encrypt
│   ├── orion_commands.h    — High-level Orion commands
│   └── orion_sniffer.h     — Passive bus sniffer
└── src/
    ├── orion_crc.c
    ├── orion_serial.c
    ├── orion_protocol.c
    ├── orion_commands.c
    └── orion_sniffer.c
```

## Comparison: Proxy vs ESP32 Gateway

| Feature | Orion Proxy (PC) | ESP32 Gateway |
|---------|------------------|---------------|
| **Hardware** | USB-RS485 adapter only | Custom PCB needed |
| **Multi-client** | ✅ TCP server (16 clients) | ❌ Single USB serial |
| **Protocol** | Handled in proxy | Handled in firmware |
| **Client simplicity** | ✅ Simple text over TCP | ✅ Simple text over serial |
| **Language support** | ✅ Any language with TCP sockets | ⚠️ Need serial library |
| **Always-on PC** | ❌ Required | ✅ Standalone |
| **WiFi AP** | ❌ Not applicable | ✅ On-demand |
| **Updates** | ✅ Just recompile | ⚠️ Flash firmware |
