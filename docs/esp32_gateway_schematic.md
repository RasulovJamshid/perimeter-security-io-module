# ESP32 Orion Gateway — Complete Schematic

## Hardware Components

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit v1 | 1 | Any ESP32 with 3 UARTs |
| MAX485 module | 1 | RS-485 transceiver |
| LED (green) | 1 | Status indicator |
| LED (red) | 1 | Debug mode indicator |
| Resistor 220Ω | 2 | For LEDs |
| Push button | 1 | Debug mode toggle (optional - can use BOOT button) |
| Resistor 10kΩ | 1 | Pull-up for external button (if not using BOOT) |
| Screw terminals | 2 | RS-485 bus connection |
| Pin headers | 1 | External serial cable connection |

## Complete Wiring Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ESP32 DevKit v1                                 │
│                                                                         │
│  ┌────────────────────────────────────────────────────────────┐        │
│  │                                                            │        │
│  │  3V3 ────────────────────────────────────┬─────────────┐  │        │
│  │                                          │             │  │        │
│  │  GND ─────────────────────────┬──────────┼─────────────┼──┼────┐   │
│  │                               │          │             │  │    │   │
│  │  GPIO16 (TX2) ────────────────┼──────────┼─────────────┼──┼──┐ │   │
│  │  GPIO17 (RX2) ────────────────┼──────────┼─────────────┼──┼┐ │ │   │
│  │  GPIO4  (DE/RE) ──────────────┼──────────┼─────────────┼──┼┼─┼─┼┐  │
│  │                               │          │             │  │││ │ ││  │
│  │  GPIO25 (TX1) ────────────────┼──────────┼─────────────┼──┼┼┼─┼─┼┼┐ │
│  │  GPIO26 (RX1) ────────────────┼──────────┼─────────────┼──┼┼┼┼┼─┼┼┼┐│
│  │                               │          │             │  │││││ │││││
│  │  GPIO2  (Status LED) ─────────┼──────────┼─────────────┼──┼┼┼┼┼─┼┼┼┼┼─┐
│  │  GPIO15 (Debug LED) ──────────┼──────────┼─────────────┼──┼┼┼┼┼┼┼┼┼┼┼┐│
│  │  GPIO0  (Debug Button) ───────┼──────────┼─────────────┼──┼┼┼┼┼┼┼┼┼┼┼┼┼─┐
│  │                               │          │             │  │││││││││││││││
│  │  USB ──────────────────────── PC for programming       │  │││││││││││││││
│  │                                                         │  │││││││││││││││
│  └─────────────────────────────────────────────────────────┘  │││││││││││││││
│                                                                │││││││││││││││
└────────────────────────────────────────────────────────────────┼┼┼┼┼┼┼┼┼┼┼┼┼┘
                                                                 │││││││││││││││
    ┌────────────────────────────────────────────────────────────┘││││││││││││││
    │  ┌──────────────────────────────────────────────────────────┘│││││││││││││
    │  │  ┌────────────────────────────────────────────────────────┘││││││││││││
    │  │  │  ┌──────────────────────────────────────────────────────┘│││││││││││
    │  │  │  │                                                        │││││││││││
    │  │  │  │  MAX485 Module                                         │││││││││││
    │  │  │  │  ┌──────────────┐                                      │││││││││││
    │  │  │  │  │              │                                      │││││││││││
    │  │  │  └──┤ DI           │                                      │││││││││││
    │  │  └─────┤ RO           │                                      │││││││││││
    │  └────────┤ DE           │                                      │││││││││││
    │           │ RE  (tied    │                                      │││││││││││
    │           │      to DE)  │                                      │││││││││││
    ├───────────┤ VCC (3.3V)   │                                      │││││││││││
    │           │              │                                      │││││││││││
    │      ┌────┤ GND          │                                      │││││││││││
    │      │    │              │                                      │││││││││││
    │      │    │ A ───────────┼──────────────────────────────────────┼┼┼┼┼┼┼┼┼┐│
    │      │    │ B ───────────┼──────────────────────────────────────┼┼┼┼┼┼┼┼┼┼┼┐
    │      │    └──────────────┘                                      ││││││││││││
    │      │                                                           ││││││││││││
    │      │                                                           ││││││││││││
    │      │    External Serial Cable (to external system)            ││││││││││││
    │      │    ┌──────────────────────────────────────┐              ││││││││││││
    │      │    │  3-pin connector                     │              ││││││││││││
    │      │    │                                      │              ││││││││││││
    │      ├────┤  GND                                 │              ││││││││││││
    │      │    │                                      │              ││││││││││││
    │      │    │  TX (GPIO25) ─────────────────────────┼──────────────┘│││││││││
    │      │    │                                      │               │││││││││
    │      │    │  RX (GPIO26) ─────────────────────────┼───────────────┘││││││││
    │      │    │                                      │                ││││││││
    │      │    └──────────────────────────────────────┘                ││││││││
    │      │                                                             ││││││││
    │      │                                                             ││││││││
    │      │    Status LED (Green) - Bus Activity                       ││││││││
    │      │    ┌─────────┐                                             ││││││││
    │      │    │   LED   │                                             ││││││││
    │      │    │  (green)│                                             ││││││││
    │      │    │    │+   │                                             ││││││││
    │      │    │    ├────┼─────────────────────────────────────────────┘│││││││
    │      │    │    │    │                                              │││││││
    │      │    │   ┌┴┐   │  220Ω resistor                              │││││││
    │      │    │   │ │   │                                              │││││││
    │      │    │   │ │   │                                              │││││││
    │      │    │   └┬┘   │                                              │││││││
    │      │    │    │    │                                              │││││││
    │      └────┼────┴────┘                                              │││││││
    │           │                                                         │││││││
    │           │                                                         │││││││
    │           │    Debug LED (Red) - Debug Mode Active                 │││││││
    │           │    ┌─────────┐                                         │││││││
    │           │    │   LED   │                                         │││││││
    │           │    │  (red)  │                                         │││││││
    │           │    │    │+   │                                         │││││││
    │           │    │    ├────┼─────────────────────────────────────────┘││││││
    │           │    │    │    │                                          ││││││
    │           │    │   ┌┴┐   │  220Ω resistor                          ││││││
    │           │    │   │ │   │                                          ││││││
    │           │    │   │ │   │                                          ││││││
    │           │    │   └┬┘   │                                          ││││││
    │           │    │    │    │                                          ││││││
    │           └────┼────┴────┘                                          ││││││
    │                │                                                    ││││││
    │                │                                                    ││││││
    │                │    Debug Button (optional - or use BOOT button)   ││││││
    │                │    ┌──────────────────────────────────┐           ││││││
    │                │    │                                  │           ││││││
    │                │    │  ┌────────┐                      │           ││││││
    │                │    │  │ Button │                      │           ││││││
    │                │    │  │  (NO)  │                      │           ││││││
    │                │    │  └───┬────┘                      │           ││││││
    │                │    │      │                           │           ││││││
    │                │    │      ├───────────────────────────┼───────────┘│││││
    │                │    │      │                           │            │││││
    │                │    │     ┌┴┐  10kΩ pull-up           │            │││││
    │                │    │     │ │  (internal on GPIO0)    │            │││││
    │                │    │     │ │                          │            │││││
    │                │    │     └┬┘                          │            │││││
    │                │    │      │                           │            │││││
    │                ├────┼──────┴───────────────────────────┘            │││││
    │                │    │                                               │││││
    │                │    └───────────────────────────────────────────────┘││││
    │                │                                                     ││││
    │                │                                                     ││││
    │                │    Orion RS-485 Bus                                ││││
    │                │    ┌──────────────────────────────────────────┐    ││││
    │                │    │                                          │    ││││
    │                │    │  A ──────────────────────────────────────┼────┘│││
    │                │    │                                          │     │││
    │                │    │  B ──────────────────────────────────────┼─────┘││
    │                │    │                                          │      ││
    │                │    │  (connects to С2000М + all devices)      │      ││
    │                │    │                                          │      ││
    │                │    │  120Ω termination at both ends of bus    │      ││
    │                │    │                                          │      ││
    │                │    └──────────────────────────────────────────┘      ││
    │                │                                                      ││
    │                └──────────────────────────────────────────────────────┘│
    │                                                                        │
    └────────────────────────────────────────────────────────────────────────┘
```

## Pin Summary Table

| ESP32 Pin | Function | Connects To | Notes |
|-----------|----------|-------------|-------|
| **RS-485 Bus (UART2)** |
| GPIO16 (TX2) | RS-485 TX | MAX485 DI | Unused in passive mode |
| GPIO17 (RX2) | RS-485 RX | MAX485 RO | Bus listener |
| GPIO4 | RS-485 DE/RE | MAX485 DE+RE | Stays LOW (receive only) |
| **External Serial (UART1)** |
| GPIO25 (TX1) | Serial TX | External system RX | 115200 baud |
| GPIO26 (RX1) | Serial RX | External system TX | 115200 baud |
| **LEDs** |
| GPIO2 | Status LED | Green LED + 220Ω | Built-in LED on most DevKits |
| GPIO15 | Debug LED | Red LED + 220Ω | Debug mode indicator |
| **Debug Button** |
| GPIO0 | Debug button | Push button to GND | BOOT button on most DevKits |
| **Power** |
| 3V3 | Power | MAX485 VCC | 3.3V supply |
| GND | Ground | All grounds | Common ground |
| USB | Power + debug | PC | Programming and monitoring |

## LED Behavior

### Status LED (Green, GPIO2)

| Pattern | Meaning |
|---------|---------|
| **Fast blink** (200ms) | Bus active, devices online |
| **Slow blink** (1000ms) | Searching for devices |
| **Solid ON** | Alarm detected on any zone |
| **OFF** | No bus activity |

### Debug LED (Red, GPIO15)

| Pattern | Meaning |
|---------|---------|
| **Solid ON** | Debug mode active (raw or verbose) |
| **OFF** | Debug mode off (normal operation) |

## Debug Button Operation

Press the debug button (GPIO0 / BOOT button) to cycle through debug modes:

1. **Press 1**: Debug OFF → All debug disabled
2. **Press 2**: Raw packet dump ON → Hex dump of every packet
3. **Press 3**: Verbose logging ON → Detailed stats every 10s
4. **Press 4**: Both raw + verbose → Full debug mode
5. **Press 5**: Cycles back to OFF

Each press is indicated by:
- Debug LED state change
- Message on USB serial
- Message on external serial cable

## External Serial Cable Wiring

### Option A: Direct TTL Connection (3.3V MCU)

```
ESP32                External MCU (STM32/Arduino/etc)
GPIO25 (TX) ────────► RX
GPIO26 (RX) ◄──────── TX
GND ────────────────── GND
```

### Option B: USB-UART Adapter (to PC)

```
ESP32                CP2102/CH340 Module
GPIO25 (TX) ────────► RX
GPIO26 (RX) ◄──────── TX
GND ────────────────── GND
                      USB ──► PC (appears as COM port)
```

### Option C: RS-232 (Industrial/PLC)

```
ESP32                MAX3232 Module          DB9 Connector
GPIO25 (TX) ────────► TIN  ───► TOUT ──────► Pin 2 (RX)
GPIO26 (RX) ◄──────── ROUT ◄─── RIN ◄────── Pin 3 (TX)
GND ────────────────── GND ────────────────► Pin 5 (GND)
3V3 ────────────────► VCC
```

## Power Supply Options

### Option 1: USB Power Only (Development)
- Connect ESP32 USB to PC or 5V USB charger
- Simple, but requires USB cable

### Option 2: External 5V (Production)
- Connect 5V to ESP32 VIN pin
- GND to ESP32 GND
- More reliable for permanent installation

### Option 3: 12V from Orion System
- Use DC-DC buck converter (12V → 5V)
- Connect 12V from Orion PSU to buck converter input
- Buck converter output (5V) to ESP32 VIN
- Shares power with security system

## RS-485 Bus Connection

**CRITICAL**: The ESP32 gateway is **passive only** — it never transmits.

```
Orion RS-485 Bus:

С2000М ──┬── Device 1 ──┬── Device 2 ──┬── ESP32 Gateway
  │      │     │         │     │         │        │
  A ─────┼─────┼─────────┼─────┼─────────┼────────┼─── A
  │      │     │         │     │         │        │
  B ─────┼─────┼─────────┼─────┼─────────┼────────┼─── B
  │      │     │         │     │         │        │
 120Ω   GND   GND       GND   GND       GND      120Ω
termination                                   termination
```

**Wiring rules:**
1. **A to A, B to B** — never cross!
2. **Twisted pair cable** — reduces noise
3. **120Ω termination** at both ends of bus (usually already present)
4. **ESP32 does NOT transmit** — DE/RE pin stays LOW
5. **Tap in parallel** — don't break the bus

## Bill of Materials (Complete)

| Item | Quantity | Approx Price | Notes |
|------|----------|--------------|-------|
| ESP32 DevKit v1 | 1 | $3 | Any ESP32 with 3 UARTs |
| MAX485 module | 1 | $0.50 | TTL RS-485 transceiver |
| LED green 3mm | 1 | $0.10 | Status indicator |
| LED red 3mm | 1 | $0.10 | Debug indicator |
| Resistor 220Ω | 2 | $0.05 | For LEDs |
| Push button (optional) | 1 | $0.20 | Can use built-in BOOT button |
| Resistor 10kΩ (optional) | 1 | $0.05 | Only if external button used |
| Screw terminals 2-pin | 1 | $0.20 | RS-485 connection |
| Pin headers 3-pin | 1 | $0.10 | External serial cable |
| Breadboard or PCB | 1 | $1-5 | For assembly |
| Wires, connectors | - | $1 | Misc |
| **Total** | | **~$5-10** | Depending on assembly method |

## Assembly Notes

1. **Use the built-in BOOT button** (GPIO0) — no external button needed
2. **Status LED (GPIO2)** is usually built-in on ESP32 DevKits — no external LED needed
3. **Only add external debug LED** (GPIO15) if you want visual debug indication
4. **Minimal build**: ESP32 + MAX485 module + wires = $3.50

## Safety & Best Practices

1. **Never connect/disconnect** while bus is powered
2. **Test on isolated system first** before connecting to production
3. **Use proper cable** (twisted pair, shielded if possible)
4. **Keep cable runs short** (<5m if possible)
5. **Avoid running near power cables** (EMI interference)
6. **Use strain relief** on all connectors
7. **Label all connections** clearly

## Troubleshooting Hardware

| Problem | Check |
|---------|-------|
| No power | USB cable, VIN voltage |
| No bus traffic | RS-485 A/B wiring, MAX485 power |
| High CRC errors | Termination resistors, cable quality, EMI |
| Debug button not working | GPIO0 connection, pull-up resistor |
| LEDs not working | Resistor values, LED polarity, GPIO pins |
| Serial cable not working | TX/RX crossover, baud rate, GND connection |
