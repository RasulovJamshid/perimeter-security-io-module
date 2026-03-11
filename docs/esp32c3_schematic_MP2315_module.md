# ESP32-C3 Super Mini Gateway — Simplified with MP2315 Module

## Using 4-Pin MP2315 Buck Converter Module

**Correction:** This design uses the **MP2315 module** (4 pins: IN, OUT, GND, EN), not the discrete IC version.

**Key Features:**
- ESP32-C3 Super Mini (built-in USB-C, $1.50)
- MP2315 buck converter **module** (4-pin, pre-assembled)
- MAX3485 RS-485 transceiver
- Transistor-driven LEDs
- Separate debug button (GPIO5)
- Complete protection circuits

**Board size:** 60x40mm
**Total cost:** ~$7-9 (even cheaper with module!)

---

## MP2315 Module (4-Pin Version)

### Module Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Input voltage** | 4.5-24V | Perfect for 12V Orion system |
| **Output voltage** | 3.3V fixed | Pre-configured |
| **Output current** | 3A max | More than enough (need ~200mA) |
| **Efficiency** | 95% | At typical loads |
| **Switching freq** | 500kHz | Internal |
| **Size** | ~17x11mm | Compact module |
| **Pins** | 4 | IN, OUT, GND, EN |

### Pin Description

| Pin | Name | Function |
|-----|------|----------|
| 1 | **IN** | 12V input (4.5-24V range) |
| 2 | **OUT** | 3.3V output (3A max) |
| 3 | **GND** | Ground (common) |
| 4 | **EN** | Enable (HIGH=on, LOW=off, leave floating or tie to IN for always-on) |

**Module includes all components:**
- ✅ MP2315 IC
- ✅ Inductor (22µH)
- ✅ Schottky diode
- ✅ Feedback resistors
- ✅ Input/output capacitors
- ✅ All SMD components pre-soldered

**You only need to connect 4 wires!**

---

## Complete Circuit Diagram (Simplified)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          POWER SUPPLY SECTION (MP2315 MODULE)                       │
│                                                                                     │
│  12V from Orion PSU                                                                 │
│      │                                                                               │
│      ├──[F1: 500mA PTC]──┬──[D1: 1N5819]──┬──[D2: SMBJ13A TVS]──┐                 │
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
│                     │    │     17x11mm pre-assembled        │                      │
│                     │    │                                  │                      │
│                     ├────┤ IN (12V input)                   │                      │
│                     │    │                                  │                      │
│                    12V   │ EN (enable) ──────────────────────┼──── 12V (always on) │
│                          │                                  │                      │
│                          │ OUT (3.3V output) ───────────────┼──────┬──────────┐    │
│                          │                                  │      │          │    │
│                          │ GND (ground) ────────────────────┼──────┼──────────┼─── │
│                          │                                  │      │          │    │
│                          └──────────────────────────────────┘      │          │    │
│                                                                    │          │    │
│                     Output Stage (optional additional filtering):  │          │    │
│                     ┌──[C2: 100µF/25V]──┬──[C3: 10µF/16V]──┐     │          │    │
│                     │                    │                  │     │          │    │
│                    GND                  GND                GND    │          │    │
│                                                                   │          │    │
│                                                                 3.3V ◄───────┴────┘
│                                                                   │
└───────────────────────────────────────────────────────────────────┼─────────────────┘
                                                                    │
┌───────────────────────────────────────────────────────────────────┼─────────────────┐
│                          ESP32-C3 SUPER MINI MODULE               │                 │
│                                                                   │                 │
│                        ┌──────────────────────────────────┐      │                 │
│                        │    ESP32-C3 Super Mini Board     │      │                 │
│                        │    (22.52 x 18mm)                │      │                 │
│                        │                                  │      │                 │
│   3.3V ────────────────┤ 3V3                              │      │                 │
│                        │                                  │      │                 │
│   3.3V ──[C4: 10µF]────┤ 3V3 (additional decoupling)     │      │                 │
│          │             │                                  │      │                 │
│         GND            │                                  │      │                 │
│                        │                                  │      │                 │
│   [C5-C7: 100nF each]  │  (3x decoupling caps near pins) │      │                 │
│                        │                                  │      │                 │
│   ┌─[R1: 10kΩ]─3.3V   │                                  │      │                 │
│   │                    │                                  │      │                 │
│   └────[SW1: Reset]────┤ EN (Enable/Reset)               │      │                 │
│        │               │                                  │      │                 │
│       GND              │                                  │      │                 │
│                        │                                  │      │                 │
│   ┌─[R2: 10kΩ]─3.3V   │                                  │      │                 │
│   │                    │                                  │      │                 │
│   └────[SW2: Debug]────┤ GPIO5 (Debug Button)            │      │                 │
│        │               │                                  │      │                 │
│   [C8: 100nF]          │                                  │      │                 │
│        │               │                                  │      │                 │
│       GND              │                                  │      │                 │
│                        │                                  │      │                 │
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
│                        │                              A ──┼───┬──[D3: TVS]──GND    │
│                        │                          (Pin 6) │   │                    │
│                        │                                  │   │                    │
│                        │                              B ──┼───┼──[D4: TVS]──GND    │
│                        │                          (Pin 7) │   │                    │
│                        │                                  │   │                    │
│                        └──────────────────────────────────┘   │                    │
│                                                               │                    │
│                                                          [C9: 100nF]               │
│                                                               │                    │
│                                                              GND                   │
│                                                               │                    │
│   To Orion Bus ◄──────────────────────────────────────────────┴─── A              │
│                                                                                    │
│   To Orion Bus ◄──────────────────────────────────────────────┬─── B              │
│                                                               │                    │
│                                                          [R3: 120Ω]               │
│                                                          (optional)                │
│                                                               │                    │
│                                                               A                    │
│                                                                                    │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          LED DRIVER SECTION (Transistor-Based)                      │
│                                                                                     │
│   STATUS LED (Green) - GPIO3                                                        │
│                                                                                     │
│   From ESP32 GPIO3 ──[R4: 220Ω]───┬──┤ Q1: 2N3904                                  │
│                                   │    NPN Transistor                              │
│                                  GND   Emitter                                     │
│                                       │ Collector                                   │
│                                       ├─── LED1 Cathode (-)                         │
│                                       │                                             │
│                                  [R5: 1kΩ]                                          │
│                                       │                                             │
│                                   LED1 Anode (+)                                    │
│                                   (Green, 3mm)                                      │
│                                       │                                             │
│                                     3.3V                                            │
│                                                                                     │
│   DEBUG LED (Red) - GPIO4                                                           │
│                                                                                     │
│   From ESP32 GPIO4 ──[R6: 220Ω]───┬──┤ Q2: 2N3904                                  │
│                                   │    NPN Transistor                              │
│                                  GND   Emitter                                     │
│                                       │ Collector                                   │
│                                       ├─── LED2 Cathode (-)                         │
│                                       │                                             │
│                                  [R7: 1kΩ]                                          │
│                                       │                                             │
│                                   LED2 Anode (+)                                    │
│                                   (Red, 3mm)                                        │
│                                       │                                             │
│                                     3.3V                                            │
│                                                                                     │
│   POWER LED (Yellow) - Always On                                                    │
│                                                                                     │
│   3.3V ──[R8: 1kΩ]── LED3 Anode (+) ── LED3 Cathode (-) ── GND                     │
│                      (Yellow, 3mm)                                                  │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          EXTERNAL CONNECTIONS                                       │
│                                                                                     │
│   SERIAL CABLE (to external system via UART0)                                       │
│   ┌──────────────────────────────────────┐                                         │
│   │  J1: 3-pin screw terminal            │                                         │
│   │  Pin 1: GND                          │                                         │
│   │  Pin 2: TX (from GPIO21)             │                                         │
│   │  Pin 3: RX (to GPIO20)               │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   RS-485 BUS (to Orion system via UART1)                                            │
│   ┌──────────────────────────────────────┐                                         │
│   │  J2: 2-pin screw terminal            │                                         │
│   │  Pin 1: A                            │                                         │
│   │  Pin 2: B                            │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   POWER INPUT (from Orion PSU)                                                      │
│   ┌──────────────────────────────────────┐                                         │
│   │  J3: 2-pin screw terminal            │                                         │
│   │  Pin 1: +12V (4.5-24V range)         │                                         │
│   │  Pin 2: GND                          │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Simplified Component List (with MP2315 Module)

### Power Supply (Much Simpler!)

| Ref | Value | Part Number | Package | Price | Notes |
|-----|-------|-------------|---------|-------|-------|
| **MP2315 Module** | 12V→3.3V | MP2315 buck module | 17x11mm | **$0.80** | Pre-assembled 4-pin module |
| F1 | 500mA | 0ZCJ0050FF2E | 1812 SMD | $0.40 | Resettable PTC fuse |
| D1 | 1N5819 | 1N5819 | DO-41 | $0.10 | Reverse polarity protection |
| D2 | SMBJ13A | SMBJ13A | SMB | $0.30 | TVS overvoltage protection |
| C1 | 100µF | ECA-1EM101 | Radial | $0.10 | Input bulk capacitor |
| C2 | 100µF | ECA-1EM101 | Radial | $0.10 | Output bulk capacitor (optional) |
| C3 | 10µF | CL21A106KAYNNNE | 0805 | $0.10 | Output filter (optional) |

**Components REMOVED (included in module):**
- ❌ MP2315S IC
- ❌ 22µH inductor
- ❌ SS34 Schottky diode
- ❌ 100kΩ feedback resistor
- ❌ 22kΩ feedback resistor
- ❌ Bootstrap capacitor

**Savings: ~$1.50 in components + easier assembly!**

### ESP32-C3 and Decoupling

| Ref | Value | Part Number | Package | Price | Notes |
|-----|-------|-------------|---------|-------|-------|
| U1 | ESP32-C3 | ESP32-C3 Super Mini | Module | $1.50 | Pre-assembled with USB-C |
| C4 | 10µF | CL21A106KAYNNNE | 0805 | $0.10 | Power decoupling |
| C5-C7 | 100nF | GRM188R71H104KA93D | 0603 | $0.15 | High-freq decoupling (3x) |
| R1 | 10kΩ | RC0805FR-0710KL | 0805 | $0.05 | EN pull-up |
| R2 | 10kΩ | RC0805FR-0710KL | 0805 | $0.05 | GPIO5 pull-up |
| SW1 | Reset | B3F-1000 | 6x6mm | $0.30 | Reset button (optional) |
| SW2 | Debug | B3F-1000 | 6x6mm | $0.30 | Debug button |
| C8 | 100nF | GRM188R71H104KA93D | 0603 | $0.05 | Debounce cap |

### RS-485 Interface

| Ref | Value | Part Number | Package | Price | Notes |
|-----|-------|-------------|---------|-------|-------|
| U2 | MAX3485 | MAX3485ESA+ | SOIC-8 | $1.20 | RS-485 transceiver |
| D3, D4 | TVS | PESD1CAN | SOT-23 | $0.20 | ESD protection (2x) |
| C9 | 100nF | GRM188R71H104KA93D | 0603 | $0.05 | Noise filter |
| R3 | 120Ω | RC0805FR-07120RL | 0805 | $0.05 | Termination (optional) |

### LED Drivers

| Ref | Value | Part Number | Package | Price | Notes |
|-----|-------|-------------|---------|-------|-------|
| Q1, Q2 | 2N3904 | 2N3904 | TO-92 | $0.10 | NPN transistors (2x) |
| R4, R6 | 220Ω | RC0805FR-07220RL | 0805 | $0.10 | Base resistors (2x) |
| R5, R7 | 1kΩ | RC0805FR-071KL | 0805 | $0.10 | LED resistors (2x) |
| R8 | 1kΩ | RC0805FR-071KL | 0805 | $0.05 | Power LED resistor |
| LED1 | Green | L-53GD | 3mm | $0.10 | Status LED |
| LED2 | Red | L-53ID | 3mm | $0.10 | Debug LED |
| LED3 | Yellow | L-53YD | 3mm | $0.10 | Power LED |

### Connectors

| Ref | Type | Part Number | Price | Notes |
|-----|------|-------------|-------|-------|
| J1 | 3-pin screw | Phoenix 1757019 | $0.30 | External serial |
| J2 | 2-pin screw | Phoenix 1757019 | $0.20 | RS-485 bus |
| J3 | 2-pin screw | Phoenix 1757019 | $0.20 | Power input |

---

## Total Cost Comparison

### With MP2315 Module (4-pin)
**Total: ~$7-9**

| Section | Cost |
|---------|------|
| ESP32-C3 Super Mini | $1.50 |
| **MP2315 module** | **$0.80** |
| Protection (F1, D1, D2) | $0.80 |
| MAX3485 + protection | $1.45 |
| LEDs, transistors, resistors | $0.85 |
| Buttons | $0.60 |
| Connectors | $0.70 |
| Capacitors | $0.65 |
| **Total** | **$7.35** |

### With Discrete MP2315 IC (6-pin)
**Total: ~$9-10**

| Section | Cost |
|---------|------|
| ESP32-C3 Super Mini | $1.50 |
| **MP2315 IC + passives** | **$1.50** |
| Protection | $0.80 |
| MAX3485 + protection | $1.45 |
| LEDs, transistors, resistors | $0.85 |
| Buttons | $0.60 |
| Connectors | $0.70 |
| Capacitors | $0.65 |
| **Total** | **$8.05** |

**Savings with module: ~$0.70 + much easier assembly!**

---

## MP2315 Module Connection

### Wiring (4 wires only!)

```
Power Input Section:
12V ──[Fuse]──[Diode]──[TVS]──[C1]──┬──► MP2315 IN
                                    │
                                   GND ──► MP2315 GND
                                    
MP2315 EN ──► 12V (or leave floating for always-on)

MP2315 OUT ──[C2]──[C3]──► 3.3V to ESP32-C3 and circuits
                  │
                 GND
```

### Step-by-Step Connection

1. **Input power:**
   - Connect 12V (after protection) to MP2315 **IN** pin
   - Connect GND to MP2315 **GND** pin

2. **Enable:**
   - Connect MP2315 **EN** pin to **IN** pin (always enabled)
   - OR leave **EN** floating (internal pull-up enables it)
   - OR connect to GPIO for software control (advanced)

3. **Output power:**
   - Connect MP2315 **OUT** pin to 3.3V rail
   - Add C2 (100µF) and C3 (10µF) for filtering (optional but recommended)
   - Connect to ESP32-C3 3V3 pin

4. **Ground:**
   - All GND pins connected together (common ground)

---

## Advantages of MP2315 Module vs Discrete IC

| Aspect | Discrete IC (6-pin) | Module (4-pin) | Winner |
|--------|---------------------|----------------|--------|
| **Price** | $1.50 (IC + parts) | **$0.80** | ✅ Module |
| **Assembly** | 8 components to solder | **4 wires** | ✅ Module |
| **PCB space** | Larger footprint | **Compact** | ✅ Module |
| **Reliability** | Depends on layout | **Pre-tested** | ✅ Module |
| **Flexibility** | Custom voltage | Fixed 3.3V | Discrete |
| **Debugging** | Complex | **Simple** | ✅ Module |

**For this project: Module is better!**

---

## Assembly Instructions (Simplified)

### Step 1: Power Supply (5 minutes)
1. Solder F1 (fuse)
2. Solder D1 (1N5819), D2 (SMBJ13A)
3. Solder C1 (100µF input cap)
4. **Solder MP2315 module** (4 pins: IN, OUT, GND, EN)
5. Connect EN to IN (always-on)
6. Solder C2, C3 (output caps)
7. **Test:** Apply 12V, measure 3.3V output ✅

### Step 2: ESP32-C3 (5 minutes)
1. Solder pin headers or directly solder ESP32-C3 module
2. Solder C4-C7 (decoupling caps) close to module
3. **Test:** Connect USB-C, verify PC recognizes device ✅

### Step 3: RS-485 (5 minutes)
1. Solder U2 (MAX3485)
2. Solder D3, D4 (TVS diodes)
3. Solder C9 (100nF)
4. Solder R3 (120Ω, if needed)

### Step 4: LEDs and Buttons (10 minutes)
1. Solder Q1, Q2 (transistors)
2. Solder R4-R8 (resistors)
3. Solder LED1, LED2, LED3 (watch polarity!)
4. Solder SW1, SW2 (buttons)
5. Solder R1, R2, C8

### Step 5: Connectors (5 minutes)
1. Solder J1, J2, J3 (screw terminals)
2. Label all connections

### Step 6: Testing (10 minutes)
1. Visual inspection
2. Continuity test
3. Power-on test (measure voltages)
4. Flash firmware via USB-C
5. Test RS-485 reception

**Total assembly time: ~40 minutes** (vs 2-3 hours with discrete IC!)

---

## Where to Buy MP2315 Module

### Recommended Suppliers

| Supplier | Part Search | Price | Shipping | Notes |
|----------|-------------|-------|----------|-------|
| **AliExpress** | "MP2315 module 3.3V" | $0.50-1.00 | 2-4 weeks | Cheapest |
| **Amazon** | "MP2315 buck converter 3.3V" | $2-3 (2-pack) | 1-2 days | Fast |
| **eBay** | "MP2315 step down 3.3V" | $0.80-1.50 | 1-3 weeks | Good price |

**Look for:**
- Output: 3.3V fixed
- Input: 4.5-24V or 4.75-23V
- Current: 3A
- 4 pins: IN, OUT, GND, EN

**Alternative modules (compatible):**
- LM2596S 3.3V module (larger, cheaper)
- XL4015 3.3V module (higher current)
- MP1584 3.3V module (smaller)

---

## PCB Layout (Simplified)

### Board Layout with Module

```
Top View (60mm × 40mm):

┌─────────────────────────────────────────────────────────┐
│                                                         │
│  [J3: Power]                    [J2: RS-485]           │
│   12V  GND                        A    B               │
│    │    │                         │    │               │
│  ┌─┴────┴─────────────────────────┴────┴─────┐         │
│  │ F1  D1  D2  C1                            │         │
│  │                                           │         │
│  │ [MP2315 Module]                           │         │
│  │  17x11mm                                  │         │
│  │  IN OUT GND EN                            │         │
│  │                                           │         │
│  │ C2  C3                                    │         │
│  └───────────────────────────────────────────┘         │
│                    │                                   │
│                  3.3V                                  │
│                    │                                   │
│  ┌─────────────────┴──────────────────┐               │
│  │  ESP32-C3 Super Mini               │               │
│  │  [U1] (on pin headers)             │               │
│  │  [C4][C5][C6][C7]                  │               │
│  │                                    │               │
│  │  USB-C ──────────────────────────► │               │
│  │                                    │               │
│  └────────────────────────────────────┘               │
│         │    │    │    │    │                         │
│        G6   G7   G2  G21  G20                         │
│         │    │    │    │    │                         │
│  ┌──────┴────┴────┴────┴────┴──────┐                  │
│  │  MAX3485 [U2]                   │                  │
│  │  [D3][D4][C9][R3]               │                  │
│  └─────────────────────────────────┘                  │
│                                                        │
│  [LED1]  [LED2]  [LED3]                               │
│  [Q1]    [Q2]                                         │
│  [R4-R8]                                              │
│                                                        │
│  [SW1]   [SW2]                                        │
│  [R1][R2][C8]                                         │
│                                                        │
│  [J1: Serial]                                         │
│   GND TX RX                                           │
│                                                        │
└─────────────────────────────────────────────────────────┘
```

**Much simpler layout!**
- MP2315 module is pre-assembled (no complex switching node routing)
- Fewer components to place
- Easier to hand-solder
- More reliable (module is factory-tested)

---

## Summary

### MP2315 Module (4-pin) is Perfect for This Project

✅ **Cheaper:** $0.80 vs $1.50 (discrete IC + parts)
✅ **Simpler:** 4 wires vs 8+ components
✅ **Faster:** 5 minutes vs 30 minutes assembly
✅ **More reliable:** Pre-tested, factory-assembled
✅ **Easier debugging:** Just check 4 connections
✅ **No PCB layout issues:** Module handles all critical traces

### When to Use Discrete IC Instead

- Need custom output voltage (not 3.3V)
- Very tight space constraints (module is 17x11mm)
- High-volume production (discrete is cheaper at 1000+ units)
- Want to learn about buck converter design

**For prototyping and small production: Use the module!**

---

## Files Updated

- `esp32c3_schematic_MP2315_module.md` - This file (corrected schematic)
- `main.cpp` - Already updated with ESP32-C3 pin definitions
- Next: Update BOM with module version

**Ready to build with the 4-pin MP2315 module!** 🚀
