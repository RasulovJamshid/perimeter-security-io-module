# Bolid Orion — PC Library & CLI Tool

C99 library for direct communication with Bolid Orion security devices via RS-485.
Compatible with Windows XP+ (MSVC 2010+, MinGW) and Linux/POSIX.

## Features

- **Direct Orion protocol** — no С2000-ПП or Modbus needed
- **CRC-8-Dallas** checksum (lookup table)
- **XOR encryption** (GLOBAL_KEY / MESSAGE_KEY)
- **Cross-platform** serial port (Win32 + POSIX)
- **Command API**: read status, arm/disarm zones, events, device info
- **Passive sniffer** — listen-only mode, safe with existing desktop app
- **Windows XP compatible** — C99, works with MSVC 2010+ / MinGW

## Building

### Requirements

- CMake 3.14+ (for XP: CMake 3.13 or earlier)
- C99-compatible compiler (GCC, MSVC 2010+, Clang, MinGW)

### Build (GCC / MinGW)

```bash
cd pc
mkdir build && cd build
cmake ..
cmake --build .
```

### Build (Visual Studio)

```cmd
cd pc
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## CLI Commands

```bash
# Read device info (unencrypted — safe first test)
./orion_example COM3 1 info

# Read device status (encrypted)
./orion_example COM3 3 status

# Set encryption key
./orion_example COM3 3 setkey 0xBA

# Arm / disarm zone
./orion_example COM3 3 arm 1
./orion_example COM3 3 disarm 1

# Read events
./orion_example COM3 3 events

# Scan bus for all devices
./orion_example COM3 1 scan

# Passive sniffer (SAFE — read-only, no transmissions)
./orion_example COM3 0 sniff
```

## Library API

```c
#include "orion_serial.h"
#include "orion_protocol.h"
#include "orion_commands.h"

/* Open serial port */
orion_serial_config_t cfg;
memset(&cfg, 0, sizeof(cfg));
cfg.port_name  = "COM3";
cfg.baud_rate  = 9600;
cfg.timeout_ms = 500;
orion_port_t port = orion_serial_open(&cfg);

/* Initialize protocol context */
orion_ctx_t ctx;
orion_ctx_init(&ctx, port, 0x00);  /* global_key = 0x00 */

/* Read device status */
orion_device_status_t status;
orion_cmd_read_status(&ctx, 3, &status);
printf("Status: %s\n", orion_status_str(status.status1));

/* Cleanup */
orion_serial_close(port);
```

## File Structure

```
pc/
├── CMakeLists.txt
├── include/
│   ├── orion_crc.h          # CRC-8-Dallas checksum
│   ├── orion_serial.h       # Serial port abstraction
│   ├── orion_protocol.h     # Packet build/parse/encrypt
│   ├── orion_commands.h     # High-level command API
│   └── orion_sniffer.h      # Passive bus sniffer
├── src/
│   ├── orion_crc.c
│   ├── orion_serial.c
│   ├── orion_protocol.c
│   ├── orion_commands.c
│   └── orion_sniffer.c
└── examples/
    └── main.c               # CLI demo application
```

## Bus Safety

| Mode | Transmits? | Safe with desktop app? |
|------|:---------:|:----------------------:|
| **sniff** | NO | YES |
| **info/status/arm/...** | YES | NO — will cause bus collisions |

For active commands, disconnect the С2000М or use a separate RS-485 segment.
