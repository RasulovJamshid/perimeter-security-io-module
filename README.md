# Bolid "Орион" (Orion) — Perimeter Security Integration

Integration toolkit for Bolid "Орион" security systems via the native Orion protocol over RS-485.
Two independent implementations: a **PC application** (C99, Windows/Linux) and an **ESP32 hardware device**.

## System Overview

```
    Orion RS-485 Bus (9600 8N1)
    ════════════════════════════════════════════════════════

     A ──┬────────┬──────────┬──────────┬──────────┬──── A
         │        │          │          │          │
     B ──┼────────┼──────────┼──────────┼──────────┼──── B
         │        │          │          │          │
    ┌────┴───┐┌───┴───┐┌────┴───┐┌────┴───┐┌────┴────────────┐
    │ С2000М ││Sig-20P││С2000-4 ││С2000-2 ││  ESP32 + MAX485 │
    │ master ││addr=1 ││addr=2  ││addr=3  ││  addr=10        │
    └────────┘└───────┘└────────┘└────────┘│  (YOUR DEVICE)  │
                                           │  WiFi ═══► MQTT  │
         ┌────────────┐                    └──────────────────┘
         │ USB-RS485  │  (sniffer tap)
         │  adapter   ├─── A/B
         └──────┬─────┘
                │ USB
         ┌──────┴─────┐
         │   PC App   │
         │  (sniffer  │
         │  or master)│
         └────────────┘
```

## Two Implementations

### 1. [`pc/`](pc/) — Win32/Linux PC Application

C99 library + CLI tool for communicating with Orion devices from a PC via USB-to-RS485 adapter.

| Feature | Details |
|---------|---------|
| **Language** | C99 (MSVC 2010+, GCC, MinGW, Clang) |
| **OS** | Windows XP+ / Linux |
| **Commands** | scan, info, status, arm/disarm, events, sniff |
| **Sniffer** | Passive listen-only mode — safe with existing desktop app |
| **Build** | CMake |

```bash
cd pc && mkdir build && cd build && cmake .. && cmake --build .
./orion_example COM3 0 sniff    # passive sniffer
./orion_example COM3 1 info     # read device info
```

See [`pc/README.md`](pc/README.md) for full build instructions and API docs.

---

### 2. [`firmware/esp32_orion_slave/`](firmware/esp32_orion_slave/) — ESP32 Gateway / Bridge

ESP32 firmware that **passively monitors the entire Orion RS-485 bus**, decodes all device traffic, and forwards statuses/events to an external system. Zero bus interference — read-only.

| Feature | Details |
|---------|---------|
| **MCU** | ESP32 (any DevKit, 3 UARTs) |
| **Transceiver** | MAX485 / SP3485 (receive only) |
| **Monitors** | ALL devices on the bus (up to 127) |
| **Serial cable** | UART1 (GPIO25/26) → external system |
| **WiFi** | MQTT + HTTP REST API |
| **Cost** | ~$4 total hardware |
| **Build** | PlatformIO or Arduino IDE |

```bash
cd firmware/esp32_orion_slave
pio run --target upload
```

See [`firmware/esp32_orion_slave/README.md`](firmware/esp32_orion_slave/README.md) for serial protocol, MQTT topics, and HTTP API.

---

## Bus Safety — Coexisting with Desktop App

| Mode | Transmits? | Safe with desktop app? | Use case |
|------|:---------:|:----------------------:|----------|
| **ESP32 gateway** | NO | YES | Monitor entire system, forward to external |
| **PC sniffer** (`sniff`) | NO | YES | Monitor all bus traffic passively from PC |
| **PC master** (`info`, `arm`, etc.) | YES | NO — bus collisions | Use on isolated segment only |

## Project Structure

```
perimiter_security/
├── README.md                          ← You are here
├── docs/
│   └── connection_diagrams.md         # Detailed wiring & schematics
├── pc/                                # Win32/Linux PC application
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── include/
│   │   ├── orion_crc.h
│   │   ├── orion_serial.h
│   │   ├── orion_protocol.h
│   │   ├── orion_commands.h
│   │   └── orion_sniffer.h
│   ├── src/
│   │   ├── orion_crc.c
│   │   ├── orion_serial.c
│   │   ├── orion_protocol.c
│   │   ├── orion_commands.c
│   │   └── orion_sniffer.c
│   └── examples/
│       └── main.c
└── firmware/                          # ESP32 hardware device
    └── esp32_orion_slave/
        ├── platformio.ini
        ├── README.md
        ├── src/
        │   └── main.cpp
        └── lib/
            └── OrionSlave/
                ├── orion_slave.h
                ├── orion_slave.cpp
                ├── orion_crc.h
                └── orion_crc.cpp
```

## Connection Diagrams

Full wiring schematics for both the PC sniffer and the ESP32 slave emulator
are in [`docs/connection_diagrams.md`](docs/connection_diagrams.md), including:

- ESP32 ↔ MAX485 wiring
- Sensor wiring (active low with pull-ups)
- Complete system schematic
- RS-485 bus rules (termination, topology, cable)
- Power options (USB, external 5V, 12V from Orion bus)

## Protocol Overview

```
Transport    : RS-485, 9600 baud, 8-N-1
Architecture : Master/Slave (С2000М is master)
Addressing   : 1..127 (broadcast = 0)
CRC          : CRC-8-Dallas (polynomial 0x31, lookup table)
Encryption   : XOR with message_key; encrypted address = addr + 0x80
```

### Packet Format

```
[address] [length] [payload...] [crc]

Unencrypted:
  address  = device address (1..127)
  length   = payload + 1 (for CRC byte)
  payload  = command + data
  crc      = CRC-8-Dallas over all preceding bytes

Encrypted:
  address  = device address + 0x80
  payload  = [key_xor] [cmd ^ msg_key]... [msg_key padding]
  key_xor  = global_key XOR message_key
```

## Supported Bolid Devices

| Device | Type Code | Description |
|--------|:---------:|-------------|
| С2000М | — | System controller (master) |
| Signal-20P | 0x34 | Alarm zone panel (20 loops) |
| С2000-4 | 0x0A | Relay/output module |
| С2000-КДЛ | 0x16 | Addressable loop controller |
| С2000-2 | — | Access controller |
| С2000-БИ | — | Indicator block |

## References

- [Habr: Orion Protocol Decryption](https://habr.com/ru/articles/563584/)
- [Bolid Official: С2000-ПП Integration](https://bolid.ru/support/articles/articles_2.html)
- [С2000М Manual (PDF)](https://bolid.ru/files/373/566/s2m_2.06.pdf)
- [cxem.net Forum: Orion Protocol Discussion](https://forum.cxem.net/index.php?/topic/105126/)
- [GitHub: Reddds/BOLID-C2000-BI](https://github.com/Reddds/BOLID-C2000-BI) — Custom С2000-БИ firmware

## Disclaimer

Based on reverse-engineered protocol information. **Not officially supported by НВП "Болид"**.
Use at your own risk. Always verify with actual hardware before production deployment.

## License

MIT
