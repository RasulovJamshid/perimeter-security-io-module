# ESP32-C3 Super Mini Gateway — Optimized Schematic (USB-Powered)

## Design Overview

**Single USB-C cable** provides power, programming, and debug serial. No external PSU needed.

**Key Features:**
- ESP32-C3 Super Mini (built-in USB-C, ~$1.50)
- ADM2483 isolated RS-485 transceiver (2.5kV galvanic isolation)
- AMS1117-3.3 LDO regulator for peripherals (from USB 5V)
- Transistor-driven LED indicators
- Debug button (GPIO5) + Mode switch (GPIO8)
- Physical MONITOR/MASTER mode selection

**Power source:** PC USB (500mA @ 5V)
**Board size:** 50x35mm (optimized, fewer components)
**Total cost:** ~$8-9 per unit

---

## Pinout Connection Table

### ESP32-C3 Super Mini — Complete GPIO Assignment

| ESP32 Pin | Direction | Connected To | Wire/Trace | Purpose |
|-----------|-----------|--------------|------------|---------|
| **USB-C** | Bidir | PC USB cable | USB cable | Power + programming + debug serial |
| **5V** | Power Out | AMS1117-3.3 VIN | 1mm trace | USB 5V → external LDO input |
| **3V3** | Power Out | — | — | ESP32 onboard 3.3V (chip only) |
| **GND** | Ground | Common GND rail | GND plane | Ground reference for all components |
| **GPIO2** | Output | ADM2483 DE + RE | signal | RS-485 direction (LOW=receive) |
| **GPIO3** | Output | R6 (220Ω) → Q1 base | signal | Status LED driver (green) |
| **GPIO4** | Output | R8 (220Ω) → Q2 base | signal | Debug/Mode LED driver (red) |
| **GPIO5** | Input | SW2 + R4 (10kΩ pullup) | signal | Debug button (active LOW) |
| **GPIO6** | Input | ADM2483 RO (RXD) | signal | RS-485 receive data |
| **GPIO7** | Output | ADM2483 DI (TXD) | signal | RS-485 transmit data |
| **GPIO8** | Input | SW3 + R11 (10kΩ pullup) | signal | Mode switch (LOW=MONITOR) |
| **GPIO18** | Bidir | USB-C D- | USB | Built-in USB serial (auto) |
| **GPIO19** | Bidir | USB-C D+ | USB | Built-in USB serial (auto) |
| **GPIO20** | Input | J1 Pin 3 (Ext RX) | signal | External serial cable RX |
| **GPIO21** | Output | J1 Pin 2 (Ext TX) | signal | External serial cable TX |
| **EN** | Input | R3 (10kΩ pullup) + SW1 | signal | Hardware reset (active LOW) |

### ADM2483 Isolated RS-485 Module — Pin Connections

| Module Pin | Connected To | Purpose |
|------------|--------------|---------|
| **VCC** | 3.3V_EXT (AMS1117 output) | Logic-side power (3.3V) |
| **GND** | ESP32 GND | Logic-side ground |
| **RXD (RO)** | ESP32 GPIO6 | Received data → ESP32 |
| **TXD (DI)** | ESP32 GPIO7 | ESP32 → data to transmit |
| **DE** | ESP32 GPIO2 | Driver enable (HIGH=transmit) |
| **RE** | ESP32 GPIO2 (tied to DE) | Receiver enable (LOW=receive) |
| **A** | J2 Pin 1 (Orion bus A) | RS-485 differential + (isolated) |
| **B** | J2 Pin 2 (Orion bus B) | RS-485 differential − (isolated) |
| **GND_ISO** | — (floating or Orion GND) | Isolated-side ground |

### AMS1117-3.3 LDO — Pin Connections

| LDO Pin | Connected To | Purpose |
|---------|--------------|---------|
| **VIN** | ESP32 5V pin | Input from USB 5V |
| **VOUT** | 3.3V_EXT rail | Regulated 3.3V for peripherals |
| **GND** | Common GND | Ground |

### 3.3V_EXT Power Rail — Loads

| Component | Current (mA) | Connection |
|-----------|-------------|------------|
| ADM2483 VCC | ~15 | 3.3V_EXT → ADM2483 VCC |
| Q1 LED circuit (green) | ~10 | 3.3V_EXT → R7 → LED1 → Q1 |
| Q2 LED circuit (red) | ~10 | 3.3V_EXT → R9 → LED2 → Q2 |
| Power LED (yellow) | ~3 | 3.3V_EXT → R10 → LED3 → GND |
| Pull-up resistors | ~1 | R3, R4, R11 to 3.3V_EXT |
| **Total** | **~39 mA** | AMS1117 capacity: 1000mA |

---

## Complete Circuit Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                    POWER MANAGEMENT (USB 5V → AMS1117 → 3.3V_EXT)                  │
│                                                                                     │
│   From ESP32 5V pin (USB 5V)                                                        │
│        │                                                                            │
│   [C12: 10µF/16V ceramic]                                                           │
│        │                                                                            │
│        ├──────────────────────────────────────────────────────┐                     │
│        │                                                      │                     │
│        │         ┌──────────────────────────────┐             │                     │
│        │         │    AMS1117-3.3 (SOT-223)     │             │                     │
│        │         │    5V→3.3V LDO Regulator     │             │                     │
│        │         │                              │             │                     │
│        ├─────────┤ VIN (5V input)               │             │                     │
│        │         │                              │             │                     │
│        │         │ VOUT (3.3V output) ───────────┼─────────────┼──► 3.3V_EXT rail   │
│        │         │                              │             │         │           │
│        │         │ GND ──────────────────────────┼──── GND     │    [C13: 22µF]     │
│        │         │                              │             │         │           │
│        │         └──────────────────────────────┘             │        GND          │
│        │                                                      │                     │
│       GND                                                    GND                    │
│                                                                                     │
│   3.3V_EXT powers: ADM2483, LEDs, pull-up resistors (~39mA total)                   │
│   ESP32 chip powered separately by onboard regulator (~80-200mA)                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          ESP32-C3 SUPER MINI MODULE                                 │
│                                                                                     │
│                        ┌──────────────────────────────────┐                        │
│                        │    ESP32-C3 Super Mini Board     │                        │
│                        │    (22.52 x 18mm)                │                        │
│                        │                                  │                        │
│   USB-C (to PC) ◄──────┤ USB-C (power + serial + prog)   │                        │
│                        │  (GPIO18/19 USB D-/D+)          │                        │
│                        │                                  │                        │
│   To AMS1117 VIN ◄─────┤ 5V  (USB 5V output)             │                        │
│                        │                                  │                        │
│                        │ 3V3 (onboard reg, ESP32 only)    │                        │
│                        │                                  │                        │
│   Common GND ◄──────────┤ GND                              │                        │
│                        │                                  │                        │
│   ┌─[R3: 10kΩ]─3.3V_EXT                                  │                        │
│   │                    │                                  │                        │
│   └────[SW1: Reset]────┤ EN (Enable/Reset)               │                        │
│        │               │                                  │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│   ┌─[R4: 10kΩ]─3.3V_EXT                                  │                        │
│   │                    │                                  │                        │
│   └────[SW2: Debug]────┤ GPIO5 (Debug Button)            │                        │
│        │               │                                  │                        │
│   [C10: 100nF]         │                                  │                        │
│        │               │                                  │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│   ┌─[R11: 10kΩ]─3.3V_EXT                                 │                        │
│   │                    │                                  │                        │
│   └──[SW3: SPDT]───────┤ GPIO8 (Mode Switch)             │                        │
│        │               │  LOW=MONITOR, HIGH=MASTER        │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│                        │  GPIO6 (RX) ◄────────────────────┼───── ADM2483 RO       │
│                        │  GPIO7 (TX) ──────────────────────┼────► ADM2483 DI       │
│                        │  GPIO2 (DE/RE) ───────────────────┼────► ADM2483 DE+RE    │
│                        │                                  │                        │
│                        │  GPIO21 (TX) ─────────────────────┼────► J1: External TX  │
│                        │  GPIO20 (RX) ◄────────────────────┼───── J1: External RX  │
│                        │                                  │                        │
│                        │  GPIO3 ───────────────────────────┼────► R6 → Q1 (LED1)   │
│                        │  GPIO4 ───────────────────────────┼────► R8 → Q2 (LED2)   │
│                        │                                  │                        │
│                        └──────────────────────────────────┘                        │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                   RS-485 INTERFACE (ADM2483 Isolated Module)                        │
│                                                                                     │
│   ╔═══════════════════════════════════════════════════════════════════╗              │
│   ║              2.5kV GALVANIC ISOLATION BARRIER                    ║              │
│   ╚═══════════════════════════════════════════════════════════════════╝              │
│                                                                                     │
│   LOGIC SIDE (ESP32 ground domain)    ║    BUS SIDE (Orion ground domain)          │
│                                        ║                                            │
│   ┌────────────────────────────────────╫────────────────────────────────┐           │
│   │      ADM2483 Isolated Module       ║                                │           │
│   │                                    ║                                │           │
│   │  3.3V_EXT ────── VCC              ║              A ─────────────────┼──► Orion A│
│   │                                    ║                                │           │
│   │  ESP32 GND ────── GND             ║              B ─────────────────┼──► Orion B│
│   │                                    ║                                │           │
│   │  ESP32 GPIO6 ◄─── RO (RXD)       ║         GND_ISO ───────────────┼──► Float  │
│   │                                    ║         (isolated ground)      │           │
│   │  ESP32 GPIO7 ───► DI (TXD)       ║                                │           │
│   │                                    ║    Built-in isolated DC-DC     │           │
│   │  ESP32 GPIO2 ───► DE ─┐           ║    powers bus side internally  │           │
│   │                       ├── tied    ║                                │           │
│   │  ESP32 GPIO2 ───► RE ─┘           ║                                │           │
│   │                                    ║                                │           │
│   └────────────────────────────────────╫────────────────────────────────┘           │
│                                        ║                                            │
│   No TVS diodes needed on bus side —   ║   ADM2483 has built-in ESD protection     │
│   isolation barrier protects ESP32     ║   ±15kV HBM on bus pins                   │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          LED DRIVER SECTION (Transistor-Based)                      │
│                                                                                     │
│   STATUS LED (Green) - GPIO3                                                        │
│                                                                                     │
│   ESP32 GPIO3 ──[R6: 220Ω]──► Q1 Base (2N3904)                                     │
│                                    │                                               │
│                                  Emitter ── GND                                    │
│                                    │                                               │
│                                  Collector                                         │
│                                    │                                               │
│                               LED1 Cathode (-)                                     │
│                                    │                                               │
│                               LED1 Anode (+) ── [R7: 1kΩ] ── 3.3V_EXT             │
│                                                  (Green, 3mm)                      │
│                                                                                     │
│   DEBUG LED (Red) - GPIO4                                                           │
│                                                                                     │
│   ESP32 GPIO4 ──[R8: 220Ω]──► Q2 Base (2N3904)                                     │
│                                    │                                               │
│                                  Emitter ── GND                                    │
│                                    │                                               │
│                                  Collector                                         │
│                                    │                                               │
│                               LED2 Cathode (-)                                     │
│                                    │                                               │
│                               LED2 Anode (+) ── [R9: 1kΩ] ── 3.3V_EXT             │
│                                                  (Red, 3mm)                        │
│                                                                                     │
│   POWER LED (Yellow) - Always On                                                    │
│                                                                                     │
│   3.3V_EXT ──[R10: 1kΩ]──► LED3 Anode (+) ── LED3 Cathode (-) ── GND              │
│                              (Yellow, 3mm)                                          │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          EXTERNAL CONNECTIONS                                       │
│                                                                                     │
│   USB-C (SINGLE CABLE — power + programming + debug serial)                         │
│   ┌──────────────────────────────────────┐                                         │
│   │  Built into ESP32-C3 Super Mini      │                                         │
│   │                                      │                                         │
│   │  Power:  USB 5V → ESP32 + AMS1117    │                                         │
│   │  Serial: USB CDC (GPIO18/19)         │                                         │
│   │  Prog:   Firmware upload via USB     │                                         │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   SERIAL CABLE (to external system — optional, active when USB disconnected)        │
│   ┌──────────────────────────────────────┐                                         │
│   │  J1: 3-pin header or screw terminal  │                                         │
│   │                                      │                                         │
│   │  Pin 1: GND ◄────────────────────────┼──── Common Ground                       │
│   │  Pin 2: TX  (from GPIO21) ◄──────────┼──── To external system RX              │
│   │  Pin 3: RX  (to GPIO20) ─────────────┼──── From external system TX            │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│   NOTE: UART0 (GPIO20/21) shared with USB. When USB connected, use USB serial.     │
│         When USB disconnected, UART0 available on GPIO20/21 for external system.    │
│                                                                                     │
│   RS-485 BUS (to Orion system — via ADM2483 isolated module)                        │
│   ┌──────────────────────────────────────┐                                         │
│   │  J2: 2-pin screw terminal            │                                         │
│   │                                      │                                         │
│   │  Pin 1: A ◄──────────────────────────┼──── Orion bus A (isolated)              │
│   │  Pin 2: B ◄──────────────────────────┼──── Orion bus B (isolated)              │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│   NOTE: A/B are galvanically isolated from ESP32. Safe ground loop protection.      │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Reference Table

### Power Supply (AMS1117-3.3 LDO from USB 5V)

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U1 | AMS1117-3.3 | AMS1117-3.3 | SOT-223 | 5V→3.3V LDO, 1A max, powers peripherals |
| C12 | 10µF | CL21A106KAYNNNE | 0805 | AMS1117 input capacitor (5V side) |
| C13 | 22µF | CL21A226MAYLNNC | 0805 | AMS1117 output capacitor (3.3V side) |

**AMS1117-3.3 Specs:**
```
Input:    5V (from ESP32 5V pin / USB)
Output:   3.3V (fixed)
Dropout:  1.1V typical (5V - 3.3V = 1.7V > 1.1V ✅)
Load:     ~39mA (ADM2483 + LEDs + pull-ups)
Max load: 1000mA (25x headroom)
Heat:     (5V - 3.3V) × 0.039A = 0.066W (negligible)
```

### ESP32-C3 Super Mini, Buttons, and Decoupling

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U2 | ESP32-C3 | ESP32-C3 Super Mini | Module | Pre-assembled board with USB-C |
| C10 | 100nF | GRM188R71H104KA93D | 0603 | Debug button debounce capacitor |
| R3 | 10kΩ | RC0805FR-0710KL | 0805 | EN pull-up to 3.3V_EXT |
| R4 | 10kΩ | RC0805FR-0710KL | 0805 | GPIO5 pull-up (debug button) to 3.3V_EXT |
| R11 | 10kΩ | RC0805FR-0710KL | 0805 | GPIO8 pull-up (mode switch) to 3.3V_EXT |
| SW1 | Reset | B3F-1000 | 6x6mm | Reset button (press → EN to GND) |
| SW2 | Debug | B3F-1000 | 6x6mm | Debug mode button (press → GPIO5 to GND) |
| SW3 | Mode | SPDT toggle | 5mm | MONITOR/MASTER switch (GPIO8 to GND or float) |

### RS-485 Interface (Isolated)

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U3 | ADM2483 | ADM2483 module | Breakout | 2.5kV isolated RS-485, 3.3V logic, built-in ESD |

**Note:** Pre-made ADM2483 module includes all necessary components (isolated DC-DC, ESD protection, decoupling). No external TVS diodes, termination resistors, or filter caps needed — all built into the module.

### LED Drivers

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| Q1, Q2 | 2N3904 | 2N3904 | TO-92 | NPN transistor for LED drive |
| R6, R8 | 220Ω | RC0805FR-07220RL | 0805 | Transistor base resistor |
| R7, R9 | 1kΩ | RC0805FR-071KL | 0805 | LED current limit resistor |
| R10 | 1kΩ | RC0805FR-071KL | 0805 | Power LED resistor |
| LED1 | Green | L-53GD | 3mm THT | Status indicator |
| LED2 | Red | L-53ID | 3mm THT | Debug mode indicator |
| LED3 | Yellow | L-53YD | 3mm THT | Power indicator |

### Connectors

| Ref | Type | Part Number | Pitch | Notes |
|-----|------|-------------|-------|-------|
| J1 | 3-pin header | Pin header 2.54mm | 2.54mm | External serial cable (GND, TX, RX) — optional |
| J2 | 2-pin screw | Phoenix 1757019 | 5mm | RS-485 bus (A, B) — via ADM2483 isolated |
| USB-C | Built-in | On ESP32-C3 board | — | Power + programming + debug serial |

**No external power connector needed** — USB-C provides all power.

---

## Pin Assignment Summary

### ESP32-C3 Super Mini GPIO Mapping

| GPIO | Function | Direction | Connects To | Notes |
|------|----------|-----------|-------------|-------|
| **UART1 (RS-485 Bus via ADM2483)** |
| GPIO6 | U1RXD | Input | ADM2483 RO | RS-485 receive |
| GPIO7 | U1TXD | Output | ADM2483 DI | RS-485 transmit |
| GPIO2 | DE/RE | Output | ADM2483 DE+RE | Direction control (LOW=receive) |
| **UART0 (External Serial / USB shared)** |
| GPIO21 | U0TXD | Output | J1 Pin 2 | External serial TX (when USB disconnected) |
| GPIO20 | U0RXD | Input | J1 Pin 3 | External serial RX (when USB disconnected) |
| **LEDs (via transistors)** |
| GPIO3 | LED | Output | R6 → Q1 base | Status LED (green) |
| GPIO4 | LED | Output | R8 → Q2 base | Debug/Mode LED (red) |
| **Buttons & Switches** |
| GPIO5 | Button | Input | SW2 + R4 pullup | Debug button (active LOW) |
| GPIO8 | Switch | Input | SW3 + R11 pullup | Mode switch (LOW=MONITOR, HIGH=MASTER) |
| EN | Reset | Input | SW1 + R3 pullup | Hardware reset (active LOW) |
| **USB (built-in, single cable)** |
| GPIO18 | USB D- | Bidir | USB-C | Power + programming + debug serial |
| GPIO19 | USB D+ | Bidir | USB-C | Power + programming + debug serial |
| **Power** |
| 5V | Power Out | — | AMS1117-3.3 VIN | USB 5V → external LDO for peripherals |
| 3V3 | Power Out | — | ESP32 chip only | Onboard regulator (not used for peripherals) |
| GND | Ground | — | Common GND | Ground reference for all components |

---

## PCB Layout Guidelines

### Board Dimensions
- **Size:** 50mm × 35mm (optimized — no 12V power section)
- **Layers:** 2 (top + bottom)
- **Thickness:** 1.6mm standard
- **Copper:** 1oz (35µm)

### Component Placement Strategy

```
Top View (50mm × 35mm):

┌───────────────────────────────────────────────┐
│                                               │
│  ┌─────────────────────────────────┐          │
│  │  ESP32-C3 Super Mini [U2]      │          │
│  │  (on pin headers)              │          │
│  │                                │          │
│  │  USB-C ◄───────────────────────┼── To PC  │
│  │  (power + serial + prog)      │          │
│  │                                │          │
│  └─────────┬──────────────────────┘          │
│            │ 5V pin                          │
│    [C12]   │                                 │
│    [U1: AMS1117-3.3]  ──► 3.3V_EXT          │
│    [C13]                      │              │
│                               │              │
│  ┌────────────────────────────┴───┐          │
│  │  ADM2483 Module [U3]          │          │
│  │  (isolated RS-485)            │   [J2]   │
│  │  VCC GND RO DI DE RE  ║  A B │──► A B   │
│  └────────────────────────────────┘          │
│                                               │
│  [LED1]  [LED2]  [LED3]    [SW1] [SW2] [SW3]│
│  Green   Red     Yellow    Reset Debug Mode  │
│  [Q1]    [Q2]              [R3]  [R4]  [R11]│
│  [R6][R7][R8][R9][R10]    [C10]              │
│                                               │
│  [J1: Serial Out]  (optional)                │
│   GND  TX  RX                                │
│                                               │
└───────────────────────────────────────────────┘
```

### Critical Layout Rules

#### 1. AMS1117-3.3 LDO Circuit
**Simple layout — no switching noise concerns:**

- **C12 (input cap):** Place within 5mm of AMS1117 VIN pin
- **C13 (output cap):** Place within 5mm of AMS1117 VOUT pin
- **GND pad:** Wide trace to GND plane (SOT-223 tab is GND)
- **Trace width:** 1mm minimum for 5V and 3.3V_EXT rails

**Layout priority:**
```
ESP32 5V pin → C12 → U1 (AMS1117) → C13 → 3.3V_EXT rail
                           ↓
                      GND plane
```

#### 2. ESP32-C3 Antenna Area
- **Keep clear zone:** 5mm around antenna (top right corner of module)
- **No copper:** Top and bottom layers in antenna area
- **No traces:** Route signals away from antenna
- **Ground plane:** Solid under module except antenna area

#### 3. Power Distribution
- **3.3V rail:** 1mm wide traces minimum
- **GND plane:** Maximum copper pour on bottom layer
- **Decoupling caps:** Place within 5mm of IC power pins
- **Via stitching:** Every 5mm around board perimeter

#### 4. RS-485 Connection (ADM2483 Module)
- **Module placement:** Near board edge for short A/B wires to J2
- **Logic signals:** Route GPIO2/6/7 directly to module pins
- **No differential pair routing needed** — A/B are on the module itself
- **Keep module away from antenna** — isolation DC-DC may emit EMI

### Layer Stack

**Top Layer:**
- Components (SMD and THT)
- Signal traces
- 3.3V power traces
- Minimal GND traces (use vias to bottom)

**Bottom Layer:**
- Solid GND plane (maximum coverage)
- Return paths for all signals
- Thermal relief for hand soldering
- Via stitching for EMI reduction

---

## Assembly Instructions

### Step 1: AMS1117-3.3 Power Section
1. Solder U1 (AMS1117-3.3) — watch orientation (tab=GND)
2. Solder C12 (10µF input cap) close to VIN
3. Solder C13 (22µF output cap) close to VOUT
4. Wire 5V trace from ESP32 5V pin pad to U1 VIN
5. **Test:** Connect ESP32 via USB, measure 3.30-3.35V on 3.3V_EXT rail

### Step 2: ESP32-C3 Module
1. Solder pin headers to PCB (if using sockets)
2. Insert ESP32-C3 Super Mini into headers
3. **Test:** Connect USB-C, verify PC recognizes device (COM port appears)

### Step 3: ADM2483 Isolated RS-485 Module
1. Mount ADM2483 module (pin headers or direct solder)
2. Wire: VCC → 3.3V_EXT, GND → common GND
3. Wire: RO → GPIO6, DI → GPIO7, DE+RE → GPIO2
4. Wire: A/B → J2 screw terminal
5. **Test:** Check continuity, no shorts between logic and bus sides

### Step 4: LEDs, Buttons, and Mode Switch
1. Solder Q1, Q2 (2N3904 transistors) — watch EBC pinout
2. Solder R6-R10 (resistors)
3. Solder LED1, LED2, LED3 — **watch polarity!**
4. Solder SW1 (reset), SW2 (debug button), SW3 (mode switch)
5. Solder R3, R4, R11 (10kΩ pull-ups to 3.3V_EXT)
6. Solder C10 (100nF debounce on GPIO5)
7. **Test:** Connect USB, yellow power LED should light

### Step 5: Connectors
1. Solder J1 (3-pin header for external serial — optional)
2. Solder J2 (2-pin screw terminal for RS-485 bus)
3. **Test:** Continuity check all connections

### Step 6: Final Testing
1. Visual inspection (no solder bridges)
2. Continuity test (GND plane, 5V rail, 3.3V_EXT rail)
3. Power-on test: USB-C → measure 5V on 5V pin, 3.3V on 3.3V_EXT
4. Flash firmware via USB-C (PlatformIO: `pio run --target upload`)
5. Open serial monitor — verify boot message appears
6. Press debug button — verify WiFi AP starts
7. Connect J2 to RS-485 bus — verify packet reception

---

## Firmware Pin Configuration

**Current pin definitions in `src/main.cpp`:**

```cpp
/* RS-485 pins (UART1) — to Orion bus via ADM2483 */
#define RS485_TX_PIN   7     /* GPIO7 - UART1 TX */
#define RS485_RX_PIN   6     /* GPIO6 - UART1 RX */
#define RS485_DE_PIN   2     /* GPIO2 - ADM2483 DE+RE (LOW = receive only) */

/* Serial output to external system (UART0) */
#define EXT_SERIAL_TX_PIN  21   /* GPIO21 - UART0 TX */
#define EXT_SERIAL_RX_PIN  20   /* GPIO20 - UART0 RX */
#define EXT_SERIAL_BAUD    115200

/* LED indicators */
#define STATUS_LED_PIN  3    /* GPIO3 - Status (green LED via Q1) */
#define DEBUG_LED_PIN   4    /* GPIO4 - Debug/Mode (red LED via Q2) */

/* Debug button (pull-up, active LOW) */
#define DEBUG_BUTTON_PIN  5  /* GPIO5 - Debug button */

/* Mode switch (pull-up, active LOW) */
#define MODE_SWITCH_PIN   8  /* GPIO8 - MONITOR/MASTER switch */
```

**UART note for ESP32-C3:**
- `Serial` = USB CDC (built-in USB serial via GPIO18/19) — always available when USB connected
- `Serial1` = UART1 (GPIO6/7) — used for RS-485 bus via ADM2483
- UART0 (GPIO20/21) shared with USB — available for external serial only when USB disconnected

---

## Power Budget (USB-Powered)

### ESP32 Onboard Regulator (powers ESP32 chip)

| Component | Current (mA) | Notes |
|-----------|--------------|-------|
| ESP32-C3 (idle) | 43 | WiFi off, light sleep |
| ESP32-C3 (active) | 80 | WiFi off, CPU active |
| ESP32-C3 (WiFi AP on) | 200 | Peak during WiFi TX |
| **Onboard regulator load** | **80-200mA** | Well within 500mA limit |

### AMS1117-3.3 (powers peripherals via 3.3V_EXT)

| Component | Current (mA) | Notes |
|-----------|--------------|-------|
| ADM2483 logic side | 15 | Isolated RS-485 module |
| LED1 (green, status) | 10 | Via Q1 transistor driver |
| LED2 (red, debug) | 10 | Via Q2 transistor driver |
| LED3 (yellow, power) | 3 | Always on, direct drive |
| Pull-up resistors (R3,R4,R11) | 1 | 3x 10kΩ to 3.3V_EXT |
| **AMS1117 load** | **~39mA** | 1000mA capacity (25x headroom) |

### USB Total Draw

| Mode | USB Current (mA) | Notes |
|------|-------------------|-------|
| Idle (WiFi off) | ~125 | ESP32 80mA + peripherals 39mA + USB ctrl 6mA |
| Active (WiFi AP on) | ~245 | ESP32 200mA + peripherals 39mA + USB ctrl 6mA |
| **USB 2.0 limit** | **500mA** | **Margin: 255-375mA** |

### AMS1117 Heat Dissipation
- Power loss: (5V - 3.3V) × 0.039A = **0.066W** (negligible)
- No heatsink needed

---

## Troubleshooting Guide

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| **No 3.3V_EXT output** | AMS1117 not working | Check 5V on ESP32 5V pin, check C12/C13, check solder joints |
| **ESP32-C3 won't boot** | USB cable issue | Try different USB-C cable (data+power, not charge-only) |
| **USB not recognized** | Driver issue | Install ESP32-C3 USB driver, try different USB port |
| **No RS-485 reception** | Wrong A/B wiring | Swap A and B connections on J2 |
| **RS-485 no data** | ADM2483 not powered | Verify 3.3V_EXT on ADM2483 VCC pin |
| **RS-485 ground loop** | Isolation broken | Verify ADM2483 GND_ISO not connected to ESP32 GND |
| **High CRC errors** | Noise on bus | Check A/B wiring, ensure twisted pair cable |
| **LEDs don't light** | Transistor wrong | Check Q1/Q2 orientation (EBC pinout for 2N3904) |
| **Power LED off** | No 3.3V_EXT | Check AMS1117 output, check R10 and LED3 polarity |
| **Debug button no effect** | Pull-up missing | Verify R4 (10kΩ) to 3.3V_EXT, check GPIO5 wiring |
| **Mode switch no effect** | Pull-up missing | Verify R11 (10kΩ) to 3.3V_EXT, check GPIO8 wiring |
| **WiFi AP won't start** | Antenna blocked | Keep 5mm clear zone around ESP32-C3 antenna area |
| **ESP32 resets randomly** | Insufficient power | Try USB 3.0 port (900mA), check for shorts |

---

## Design Summary

### USB-Powered Architecture
- **Single USB-C cable** — power + programming + debug serial
- **AMS1117-3.3 LDO** — robust 3.3V for peripherals (39mA load, 1A capacity)
- **ADM2483 isolated RS-485** — 2.5kV galvanic isolation, safe ground loop protection
- **No external PSU** — eliminates 12V power section entirely

### Component Count (Optimized)

| Category | Components | Count |
|----------|-----------|-------|
| Power | AMS1117-3.3, C12, C13 | 3 |
| MCU | ESP32-C3 Super Mini | 1 |
| RS-485 | ADM2483 module | 1 |
| LEDs | Q1, Q2, R6-R10, LED1-3 | 10 |
| Buttons | SW1-SW3, R3, R4, R11, C10 | 7 |
| Connectors | J1, J2 | 2 |
| **Total** | | **24 components** |

### Key Specs
- **Board size:** 50x35mm
- **Total cost:** ~$8-9 per unit (including PCB)
- **Power draw:** 125-245mA from USB 2.0 (500mA available)
- **Isolation:** 2.5kV between ESP32 and Orion RS-485 bus

---

## Files and Documentation

- `docs/BOM_complete.md` — Complete bill of materials
- `docs/esp32c3_schematic_final.md` — This file (circuit diagram + pinout)
- `firmware/esp32_orion_slave/src/main.cpp` — Firmware (pin definitions match this schematic)

---

## Next Steps

1. **Order components** — See `BOM_complete.md` (ADM2483 module is the key part)
2. **Design PCB** — Use KiCad, follow layout guidelines above
3. **Fabricate PCB** — JLCPCB ($5 for 5 boards, 50x35mm 2-layer)
4. **Assemble** — Follow assembly instructions (6 steps)
5. **Flash firmware** — `pio run --target upload` via USB-C
6. **Test** — Verify USB serial, WiFi AP, RS-485 reception
7. **Deploy** — Connect to Orion bus via J2, monitor via USB serial

**Estimated time:** 1-2 hours assembly + 30min flash + 30min testing
**Total cost:** ~$8-9 per unit (including PCB fabrication)
