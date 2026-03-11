# ESP32-C3 Super Mini Gateway — Final Schematic

## Optimized Design with MP2315 Buck Converter

**Key Features:**
- ESP32-C3 Super Mini (built-in USB-C, $1.50)
- MP2315 buck converter (95% efficient, tiny footprint)
- MAX3485 RS-485 transceiver (3.3V compatible)
- Transistor-driven LEDs
- Separate debug button (GPIO5)
- Complete protection circuits

**Board size:** 60x40mm (33% smaller than previous design)
**Total cost:** ~$9-10 (professional PCB build)

---

## Complete Circuit Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          POWER SUPPLY SECTION (MP2315)                              │
│                                                                                     │
│  12V from Orion PSU                                                                 │
│      │                                                                               │
│      ├──[F1: 500mA PTC]──┬──[D1: 1N5819]──┬──[D3: SMBJ13A TVS]──┐                 │
│      │                   │                 │                      │                 │
│     GND                  │                GND                     │                 │
│                          │                                        │                 │
│                     [C1: 100µF/25V]                               │                 │
│                          │                                        │                 │
│                         GND                                       │                 │
│                                                                   │                 │
│                     ┌─────────────────────────────────────────────┘                 │
│                     │                                                               │
│                     │    ┌──────────────────────────────────┐                      │
│                     │    │     MP2315 Module (4-pin)        │                      │
│                     │    │     12V→3.3V Buck Converter      │                      │
│                     │    │     17x11mm Pre-assembled        │                      │
│                     │    │                                  │                      │
│                     ├────┤ IN  (12V input)                  │                      │
│                     │    │                                  │                      │
│                    12V   │ EN  (enable) ─────────────────────┼──── 12V (always on) │
│                          │                                  │                      │
│                          │ OUT (3.3V output) ────────────────┼──────┬──────────┐   │
│                          │                                  │      │          │   │
│                          │ GND (ground) ─────────────────────┼──────┼──────────┼─── GND
│                          │                                  │      │          │   │
│                          └──────────────────────────────────┘      │          │   │
│                                                                    │          │   │
│                     Output Filtering (optional):                   │          │   │
│                     ┌──[C2: 100µF/25V]──┬──[C3: 10µF/16V]──┐     │          │   │
│                     │                    │                  │     │          │   │
│                    GND                  GND                GND    │          │   │
│                                                                   │          │   │
│                                                                 3.3V ◄───────┴───┘
│                                                                   │
└────────────────────────────────────────────────────────────────────────────┼───────┘
                                                                             │
┌────────────────────────────────────────────────────────────────────────────┼───────┐
│                          ESP32-C3 SUPER MINI MODULE                        │       │
│                                                                            │       │
│                        ┌──────────────────────────────────┐               │       │
│                        │    ESP32-C3 Super Mini Board     │               │       │
│                        │    (22.52 x 18mm)                │               │       │
│                        │                                  │               │       │
│   3.3V ────────────────┤ 3V3                              │               │       │
│                        │                                  │               │       │
│   3.3V ──[C6: 10µF]────┤ 3V3 (additional decoupling)     │               │       │
│          │             │                                  │               │       │
│         GND            │                                  │               │       │
│                        │                                  │               │       │
│   [C7-C9: 100nF each]  │  (3x decoupling caps near pins) │               │       │
│                        │                                  │               │       │
│   ┌─[R3: 10kΩ]─3.3V   │                                  │               │       │
│   │                    │                                  │               │       │
│   └────[SW1: Reset]────┤ EN (Enable/Reset)               │               │       │
│        │               │                                  │               │       │
│       GND              │                                  │               │       │
│                        │                                  │               │       │
│   ┌─[R4: 10kΩ]─3.3V   │                                  │               │       │
│   │                    │                                  │               │       │
│   └────[SW2: Debug]────┤ GPIO5 (Debug Button)            │               │       │
│        │               │                                  │               │       │
│   [C10: 100nF]         │                                  │               │       │
│        │               │                                  │               │       │
│       GND              │                                  │               │       │
│                        │                                  │               │       │
│                        │  GPIO6 (RX) ◄────────────────────┼───── From MAX3485 RO  │
│                        │  GPIO7 (TX) ──────────────────────┼────► To MAX3485 DI    │
│                        │  GPIO2 (DE/RE) ───────────────────┼────► To MAX3485 DE+RE │
│                        │                                  │      (held LOW)        │
│                        │                                  │                        │
│                        │  GPIO21 (TX) ─────────────────────┼────► External Serial  │
│                        │  GPIO20 (RX) ◄────────────────────┼───── External Serial  │
│                        │                                  │                        │
│                        │  GPIO3 ───────────────────────────┼────► Status LED Q1    │
│                        │  GPIO4 ───────────────────────────┼────► Debug LED Q2     │
│                        │                                  │                        │
│                        │  USB-C ◄──────────────────────────┼───── To PC (prog)     │
│                        │  (GPIO18/19)                     │                        │
│                        │                                  │                        │
│                        │  GND ─────────────────────────────┼────► Common GND       │
│                        │                                  │                        │
│                        └──────────────────────────────────┘                        │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          RS-485 INTERFACE (MAX3485)                                 │
│                                                                                     │
│                        ┌──────────────────────────────────┐                        │
│                        │         MAX3485                  │                        │
│                        │        (SOIC-8)                  │                        │
│                        │                                  │                        │
│   3.3V ────────────────┤ VCC (Pin 8)                      │                        │
│                        │                                  │                        │
│   GND ─────────────────┤ GND (Pin 5)                      │                        │
│                        │                                  │                        │
│   From ESP32 GPIO7 ────┤ DI (Pin 4) Data In               │                        │
│                        │                                  │                        │
│   To ESP32 GPIO6 ◄─────┤ RO (Pin 1) Receiver Out          │                        │
│                        │                                  │                        │
│   From ESP32 GPIO2 ────┤ DE (Pin 3) Driver Enable         │                        │
│                        │                                  │                        │
│   From ESP32 GPIO2 ────┤ RE (Pin 2) Receiver Enable       │                        │
│                        │                                  │                        │
│                        │                              A ──┼───┬──[D4: TVS]──GND    │
│                        │                          (Pin 6) │   │                    │
│                        │                                  │   │                    │
│                        │                              B ──┼───┼──[D5: TVS]──GND    │
│                        │                          (Pin 7) │   │                    │
│                        │                                  │   │                    │
│                        └──────────────────────────────────┘   │                    │
│                                                               │                    │
│                                                          [C11: 100nF]              │
│                                                               │                    │
│                                                              GND                   │
│                                                               │                    │
│                                                               │                    │
│   To Orion Bus ◄──────────────────────────────────────────────┴─── A (Pin 1)      │
│                                                                                    │
│   To Orion Bus ◄──────────────────────────────────────────────┬─── B (Pin 2)      │
│                                                               │                    │
│                                                          [R5: 120Ω]               │
│                                                          (optional,                │
│                                                           only if at               │
│                                                           bus end)                 │
│                                                               │                    │
│                                                               A                    │
│                                                                                    │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          LED DRIVER SECTION (Transistor-Based)                      │
│                                                                                     │
│   STATUS LED (Green) - GPIO3                                                        │
│                                                                                     │
│   From ESP32 GPIO3 ──[R6: 220Ω]───┬──┤ Q1: 2N3904                                  │
│                                   │    NPN Transistor                              │
│                                  GND   Emitter                                     │
│                                       │                                             │
│                                       │ Collector                                   │
│                                       ├─── LED1 Cathode (-)                         │
│                                       │                                             │
│                                  [R7: 1kΩ]                                          │
│                                       │                                             │
│                                   LED1 Anode (+)                                    │
│                              │   DEBUG BUTTON (separate from BOOT button)                                      │
│                                                                                     │
│   GPIO5 ──┬──[SW1: Push button]── GND                                              │
│           │                                                                         │
│        [R8: 10kΩ pull-up to 3.3V]                                                  │
│                                                                                     │
│   Press to cycle debug modes: OFF → RAW → VERBOSE → FULL                           │
│                                                                                     │
│   MODE SWITCH (MONITOR / MASTER selection)                                         │
│                                                                                     │
│   GPIO8 ──┬──[SW2: SPDT toggle switch]──┬── GND (MONITOR position)                │
│           │                              │                                         │
│        [R11: 10kΩ pull-up to 3.3V]      └── Open (MASTER position)                │
│                                                                                     │
│   Switch DOWN (to GND)  = MONITOR mode (safe, passive listening)                   │
│   Switch UP   (open)    = MASTER mode (active control, polls devices)              │       │
│                                   │    NPN Transistor                              │
│                                  GND   Emitter                                     │
│                                       │                                             │
│                                       │ Collector                                   │
│                                       ├─── LED2 Cathode (-)                         │
│                                       │                                             │
│                                  [R9: 1kΩ]                                          │
│                                       │                                             │
│                                   LED2 Anode (+)                                    │
│                                   (Red, 3mm)                                        │
│                                       │                                             │
│                                     3.3V                                            │
│                                                                                     │
│   POWER LED (Yellow) - Always On                                                    │
│                                                                                     │
│   3.3V ──[R10: 1kΩ]── LED3 Anode (+) ── LED3 Cathode (-) ── GND                    │
│                       (Yellow, 3mm)                                                 │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          EXTERNAL CONNECTIONS                                       │
│                                                                                     │
│   SERIAL CABLE (to external system via UART0)                                       │
│   ┌──────────────────────────────────────┐                                         │
│   │  J1: 3-pin screw terminal            │                                         │
│   │                                      │                                         │
│   │  Pin 1: GND ◄────────────────────────┼──── Common Ground                       │
│   │                                      │                                         │
│   │  Pin 2: TX (from GPIO21) ◄───────────┼──── To external system RX              │
│   │                                      │                                         │
│   │  Pin 3: RX (to GPIO20) ──────────────┼──── From external system TX            │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   RS-485 BUS (to Orion system via UART1)                                            │
│   ┌──────────────────────────────────────┐                                         │
│   │  J2: 2-pin screw terminal            │                                         │
│   │                                      │                                         │
│   │  Pin 1: A ◄──────────────────────────┼──── To Orion bus A                      │
│   │                                      │                                         │
│   │  Pin 2: B ◄──────────────────────────┼──── To Orion bus B                      │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   POWER INPUT (from Orion PSU or external 12V)                                      │
│   ┌──────────────────────────────────────┐                                         │
│   │  J3: 2-pin screw terminal            │                                         │
│   │                                      │                                         │
│   │  Pin 1: +12V ◄───────────────────────┼──── 12V supply (9-24V range)            │
│   │                                      │                                         │
│   │  Pin 2: GND ◄────────────────────────┼──── Ground                              │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   USB-C PROGRAMMING (built into ESP32-C3 Super Mini)                                │
│   ┌──────────────────────────────────────┐                                         │
│   │  USB-C connector on board            │                                         │
│   │                                      │                                         │
│   │  Connected to ESP32-C3 GPIO18/19     │                                         │
│   │  (USB D-/D+)                         │                                         │
│   │                                      │                                         │
│   │  Used for firmware upload and        │                                         │
│   │  serial debugging                    │                                         │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Reference Table

### Power Supply (MP2315 Circuit)

| Ref | Value | Part Number | Package | Voltage/Current | Notes |
|-----|-------|-------------|---------|-----------------|-------|
| F1 | 500mA | 0ZCJ0050FF2E | 1812 SMD | 15V | Resettable PTC fuse |
| D1 | 1N5819 | 1N5819 | DO-41 | 40V, 1A | Reverse polarity protection |
| D2 | SS34 | SS34 | SMA | 40V, 3A | Freewheeling diode for MP2315 |
| D3 | SMBJ13A | SMBJ13A | SMB | 13V, 600W | TVS overvoltage protection |
| U1 | MP2315S | MP2315S | TSOT23-6 | 4.5-24V in, 3.3V out | Buck converter IC |
| L1 | 22µH | SRN6045-220M | 6x6mm | 3A, DCR<50mΩ | Power inductor |
| C1 | 100µF | ECA-1EM101 | Radial | 25V | Input bulk capacitor |
| C2 | 100µF | ECA-1EM101 | Radial | 25V | Output bulk capacitor |
| C3 | 22µF | CL21A226MAYLNNC | 0805 | 16V, X7R | Output ceramic cap |
| C4 | 100nF | GRM188R71H104KA93D | 0603 | 50V, X7R | Bootstrap capacitor |
| C5 | 10µF | CL21A106KAYNNNE | 0805 | 16V, X7R | Output filter |
| R1 | 100kΩ | RC0805FR-07100KL | 0805 | 1% | Feedback divider upper |
| R2 | 22kΩ | RC0805FR-0722KL | 0805 | 1% | Feedback divider lower |

**MP2315 Output Voltage:**
```
Vout = 0.6V × (1 + R1/R2)
Vout = 0.6V × (1 + 100kΩ/22kΩ)
Vout = 0.6V × 5.545
Vout = 3.33V ✅
```

### ESP32-C3 Super Mini and Decoupling

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U2 | ESP32-C3 | ESP32-C3 Super Mini | Module | Pre-assembled board with USB-C |
| C6 | 10µF | CL21A106KAYNNNE | 0805 | Additional power decoupling |
| C7-C9 | 100nF | GRM188R71H104KA93D | 0603 | High-freq decoupling (3x) |
| R3 | 10kΩ | RC0805FR-0710KL | 0805 | EN pull-up |
| R4 | 10kΩ | RC0805FR-0710KL | 0805 | GPIO5 pull-up (debug button) |
| SW1 | Reset | B3F-1000 | 6x6mm | Reset button (optional) |
| SW2 | Debug | B3F-1000 | 6x6mm | Debug mode button |
| C10 | 100nF | GRM188R71H104KA93D | 0603 | Debug button debounce |

### RS-485 Interface

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U3 | MAX3485 | MAX3485ESA+ | SOIC-8 | 3.3V RS-485 transceiver |
| D4, D5 | TVS | PESD1CAN | SOT-23 | ESD protection on A, B lines |
| C11 | 100nF | GRM188R71H104KA93D | 0603 | Noise filtering across A-B |
| R5 | 120Ω | RC0805FR-07120RL | 0805 | Optional termination resistor |

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
| J1 | 3-pin screw | Phoenix 1757019 | 5mm | External serial cable |
| J2 | 2-pin screw | Phoenix 1757019 | 5mm | RS-485 bus |
| J3 | 2-pin screw | Phoenix 1757019 | 5mm | Power input |

---

## Pin Assignment Summary

### ESP32-C3 Super Mini GPIO Mapping

| GPIO | Function | Direction | Connects To | Notes |
|------|----------|-----------|-------------|-------|
| **UART1 (RS-485 Bus)** |
| GPIO6 | U1RXD | Input | MAX3485 RO | RS-485 receive |
| GPIO7 | U1TXD | Output | MAX3485 DI | RS-485 transmit (unused) |
| GPIO2 | DE/RE | Output | MAX3485 DE+RE | Held LOW (receive only) |
| **UART0 (External Serial)** |
| GPIO21 | U0TXD | Output | External RX | Serial cable TX |
| GPIO20 | U0RXD | Input | External TX | Serial cable RX |
| **LEDs (via transistors)** |
| GPIO3 | LED | Output | Q1 base | Status LED (green) |
| GPIO4 | LED | Output | Q2 base | Debug LED (red) |
| **Buttons** |
| GPIO5 | Button | Input | SW2 | Debug button (pull-up) |
| EN | Reset | Input | SW1 | Hardware reset (pull-up) |
| **USB (built-in)** |
| GPIO18 | USB D- | Bidir | USB-C | Programming/debug |
| GPIO19 | USB D+ | Bidir | USB-C | Programming/debug |
| **Power** |
| 3V3 | Power | Input | MP2315 output | 3.3V supply |
| GND | Ground | - | Common GND | Ground reference |

---

## PCB Layout Guidelines

### Board Dimensions
- **Size:** 60mm × 40mm
- **Layers:** 2 (top + bottom)
- **Thickness:** 1.6mm standard
- **Copper:** 1oz (35µm)

### Component Placement Strategy

```
Top View (60mm × 40mm):

┌─────────────────────────────────────────────────────────┐
│                                                         │
│  [J3: Power In]              [J2: RS-485]              │
│   12V  GND                    A    B                    │
│    │    │                     │    │                    │
│  ┌─┴────┴─────────────────────┴────┴─────┐             │
│  │ F1  D1  D3                             │             │
│  │                                        │             │
│  │ MP2315 Circuit:                        │             │
│  │ [U1] [L1] [D2]                         │             │
│  │ [C1][C2][C3][C4][C5]                   │             │
│  │ [R1][R2]                               │             │
│  └────────────────────────────────────────┘             │
│                    │                                    │
│                  3.3V                                   │
│                    │                                    │
│  ┌─────────────────┴──────────────────┐                │
│  │  ESP32-C3 Super Mini               │                │
│  │  [U2] (on pin headers)             │                │
│  │  [C6][C7][C8][C9]                  │                │
│  │                                    │                │
│  │  USB-C ──────────────────────────► │  [Programming] │
│  │                                    │                │
│  └────────────────────────────────────┘                │
│         │    │    │    │    │                          │
│        G6   G7   G2  G21  G20                          │
│         │    │    │    │    │                          │
│  ┌──────┴────┴────┴────┴────┴──────┐                   │
│  │  MAX3485 [U3]                   │                   │
│  │  [D4][D5][C11][R5]              │                   │
│  └─────────────────────────────────┘                   │
│                                                         │
│  [LED1]  [LED2]  [LED3]                                │
│  Green   Red     Yellow                                │
│  [Q1]    [Q2]                                          │
│  [R6-R10]                                              │
│                                                         │
│  [SW1]   [SW2]                                         │
│  Reset   Debug                                         │
│  [R3]    [R4][C10]                                     │
│                                                         │
│  [J1: Serial Out]                                      │
│   GND  TX  RX                                          │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Critical Layout Rules

#### 1. MP2315 Switching Circuit
**Most critical section - follow these rules strictly:**

- **SW pin trace:** <10mm length, 1mm width minimum
- **D2 placement:** Within 5mm of SW pin
- **L1 placement:** Within 10mm of SW pin
- **C3 placement:** Close to output, wide trace to load
- **Ground plane:** Solid under IC, thermal relief on GND pin
- **FB trace:** Route away from SW node, shield with GND

**Layout priority:**
```
U1 (MP2315) → D2 (SS34) → L1 (22µH) → C3 (22µF) → Load
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

#### 4. RS-485 Differential Pair
- **Trace width:** 0.3mm
- **Spacing:** 0.3mm (for 120Ω differential impedance)
- **Length matching:** ±1mm
- **Route together:** Minimize vias, avoid splits

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

### Step 1: Power Supply Section
1. Solder F1 (fuse), D1 (1N5819), D3 (SMBJ13A)
2. Solder C1 (100µF input cap)
3. Solder U1 (MP2315S) - watch orientation!
4. Solder D2 (SS34) close to SW pin
5. Solder L1 (22µH inductor)
6. Solder R1, R2 (feedback divider)
7. Solder C2, C3, C4, C5 (output caps)
8. **Test:** Apply 12V, measure 3.30-3.35V output

### Step 2: ESP32-C3 Module
1. Solder pin headers to PCB (if using sockets)
2. Insert ESP32-C3 Super Mini into headers
3. Solder C6, C7, C8, C9 (decoupling caps) close to module
4. **Test:** Connect USB-C, verify PC recognizes device

### Step 3: RS-485 Interface
1. Solder U3 (MAX3485) - watch pin 1 orientation
2. Solder D4, D5 (TVS diodes)
3. Solder C11 (100nF filter cap)
4. Solder R5 (120Ω termination, if needed)
5. **Test:** Check continuity, no shorts

### Step 4: LEDs and Buttons
1. Solder Q1, Q2 (transistors) - watch EBC pinout
2. Solder R6-R10 (resistors)
3. Solder LED1, LED2, LED3 - **watch polarity!**
4. Solder SW1, SW2 (buttons)
5. Solder R3, R4 (pull-ups), C10 (debounce)
6. **Test:** Apply power, yellow LED should light

### Step 5: Connectors
1. Solder J1, J2, J3 (screw terminals)
2. Label connections clearly
3. **Test:** Continuity check all connections

### Step 6: Final Testing
1. Visual inspection (no solder bridges)
2. Continuity test (GND plane, power rails)
3. Power-on test (measure voltages)
4. Flash test firmware via USB-C
5. Test serial output
6. Connect to RS-485 bus and verify reception

---

## Firmware Pin Configuration

**Update `main.cpp` with new pin definitions:**

```cpp
/* ═══════════════════════════════════════════════════════════════════════
 *  PIN DEFINITIONS — ESP32-C3 Super Mini
 * ═══════════════════════════════════════════════════════════════════════ */

/* RS-485 pins (UART1) — to Orion bus via MAX3485 */
#define RS485_TX_PIN   7     /* GPIO7 - UART1 TX (unused in passive mode) */
#define RS485_RX_PIN   6     /* GPIO6 - UART1 RX */
#define RS485_DE_PIN   2     /* GPIO2 - MAX3485 DE+RE (held LOW = receive only) */

/* Serial output to external system (UART0) — for cable connection */
#define EXT_SERIAL_TX_PIN  21   /* GPIO21 - UART0 TX */
#define EXT_SERIAL_RX_PIN  20   /* GPIO20 - UART0 RX */
#define EXT_SERIAL_BAUD    115200

/* LED indicators */
#define STATUS_LED_PIN  3    /* GPIO3 - Status indicator (green LED via Q1) */
#define DEBUG_LED_PIN   4    /* GPIO4 - Debug mode indicator (red LED via Q2) */

/* Debug button (pull-up, active LOW) */
#define DEBUG_BUTTON_PIN  5  /* GPIO5 - Separate debug button */

/* UART initialization in setup() */
void setup() {
    // USB serial (UART0) for debugging
    Serial.begin(115200);
    
    // RS-485 bus (UART1)
    Serial1.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    // Note: External serial uses same UART0 as USB
    // When USB is connected, external serial is unavailable
    // When USB is disconnected, UART0 is available for external serial
    
    // ... rest of setup code
}
```

**Important UART note for ESP32-C3:**
- ESP32-C3 has only 2 UARTs (UART0 and UART1)
- UART0 is shared between USB and external serial
- When USB is connected, external serial won't work
- For production, disconnect USB and use UART0 for external serial

---

## Power Budget (ESP32-C3 + MP2315)

| Component | Current (mA) | Notes |
|-----------|--------------|-------|
| ESP32-C3 (idle) | 43 | WiFi off, light sleep |
| ESP32-C3 (active) | 80 | WiFi off, CPU active |
| ESP32-C3 (WiFi TX) | 180 | Peak during transmission |
| MAX3485 | 0.5 | Receive mode |
| LEDs (3x @ 2mA) | 6 | Status, debug, power |
| **Total (idle)** | **50mA** | Typical operation |
| **Total (WiFi on)** | **187mA** | Peak with WiFi |

**12V input current (at 95% MP2315 efficiency):**
- Idle: 50mA × 3.3V / 12V / 0.95 = **14.5mA**
- WiFi on: 187mA × 3.3V / 12V / 0.95 = **54mA**

**Power consumption:**
- Idle: 50mA × 3.3V = **165mW**
- WiFi on: 187mA × 3.3V = **617mW**

**MP2315 heat dissipation:**
- Power loss = 617mW × (1 - 0.95) = **31mW**
- Temperature rise = 31mW × 150°C/W = **4.6°C** (no heatsink needed!)

---

## Troubleshooting Guide

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| **No 3.3V output** | MP2315 not working | Check 12V input, check R1/R2 values, check L1 orientation |
| **3.3V too low (<3.2V)** | Wrong feedback resistors | Verify R1=100kΩ, R2=22kΩ |
| **3.3V too high (>3.4V)** | Wrong feedback resistors | Check R1/R2 solder joints |
| **ESP32-C3 won't boot** | Insufficient decoupling | Add more caps, check C6-C9 placement |
| **USB not recognized** | USB pins damaged | Check GPIO18/19, try different cable |
| **No RS-485 reception** | Wrong A/B wiring | Swap A and B connections |
| **High CRC errors** | Noise on bus | Add ferrite beads, check grounding |
| **LEDs don't light** | Transistor not switching | Check Q1/Q2 orientation (EBC pinout) |
| **Debug button not working** | Wrong GPIO or no pull-up | Verify GPIO5, check R4 (10kΩ) |
| **WiFi won't connect** | Antenna issue | Check clear zone around antenna |
| **MP2315 overheating** | Wrong inductor or short | Check L1 value (22µH), check for shorts |

---

## Advantages Summary

### ESP32-C3 Super Mini
✅ **$1.50** vs $3-4 (50% cheaper)
✅ **43mA idle** vs 80mA (46% less power)
✅ **Built-in USB-C** (no external programmer)
✅ **Smaller size** (22.52x18mm)
✅ **Modern RISC-V** architecture

### MP2315 Buck Converter
✅ **95% efficient** vs 92% (LM2596S)
✅ **$0.30** vs $1.20 (75% cheaper)
✅ **Tiny footprint** (TSOT23-6)
✅ **500kHz switching** (smaller passives)
✅ **Cool operation** (<5°C rise)

### Overall Design
✅ **60x40mm PCB** vs 80x50mm (33% smaller)
✅ **$9-10 total cost** vs $15-22 (40% cheaper)
✅ **Lower power** (50mA vs 90mA idle)
✅ **Professional quality** with protection circuits

---

## Files and Documentation

- `BOM_ESP32C3_optimized.md` - Complete bill of materials
- `esp32c3_schematic_final.md` - This file (circuit diagram)
- `main.cpp` - Firmware (needs pin updates for ESP32-C3)

---

## Next Steps

1. **Order components** - See BOM_ESP32C3_optimized.md
2. **Design PCB** - Use KiCad, follow layout guidelines
3. **Fabricate PCB** - JLCPCB ($5 for 5 boards)
4. **Assemble** - Follow assembly instructions
5. **Update firmware** - Change pin definitions for ESP32-C3
6. **Test** - Follow testing procedure
7. **Deploy** - Install and connect to Orion system

**Estimated time:** 2-3 hours assembly + 1 hour firmware update + 1 hour testing

**Total cost:** ~$10 per unit (including PCB)
