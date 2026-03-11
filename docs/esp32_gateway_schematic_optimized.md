# ESP32 Orion Gateway — Optimized Schematic

## Professional Design with Proper Power Management

This schematic includes:
- ✅ 12V to 3.3V buck converter (efficient power regulation)
- ✅ Reverse polarity protection (Schottky diode)
- ✅ Overvoltage protection (TVS diode)
- ✅ Overcurrent protection (resettable fuse)
- ✅ Transistor-driven LEDs (proper current control)
- ✅ Separate debug button (GPIO13, not BOOT button)
- ✅ Proper decoupling capacitors
- ✅ ESD protection on RS-485 lines

---

## Complete Circuit Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          POWER SUPPLY SECTION                                       │
│                                                                                     │
│  12V from Orion PSU                                                                 │
│      │                                                                               │
│      ├──[F1: 500mA PTC Fuse]──┬──[D1: 1N5819 Schottky]──┬──[D2: SMBJ13A TVS]──┐   │
│      │                        │                          │                      │   │
│     GND                       │                         GND                     │   │
│                               │                                                 │   │
│                          [C1: 100µF/25V]                                        │   │
│                               │                                                 │   │
│                              GND                                                │   │
│                                                                                 │   │
│                        ┌──────────────────┐                                     │   │
│                        │   LM2596S Buck   │                                     │   │
│                        │   Converter      │                                     │   │
│                    ┌───┤ VIN          VOUT├───┬──[C2: 100µF/16V]──┬──[C3: 10µF]──┐ │
│                    │   │                  │   │                    │              │ │
│                    │   │ GND          GND ├───┼────────────────────┼──────────────┼─┤
│                    │   └──────────────────┘   │                    │              │ │
│                    │                          GND                 GND             │ │
│                    │                                                              │ │
│                   12V                                                           3.3V│
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          ESP32 MODULE SECTION                                       │
│                                                                                     │
│                        ┌──────────────────────────────────┐                        │
│                        │       ESP32-WROOM-32             │                        │
│                        │                                  │                        │
│   3.3V ────────────────┤ VDD (3.3V)                       │                        │
│   3.3V ──[C4: 10µF]─┬──┤ VDD                              │                        │
│                     │  │                                  │                        │
│   3.3V ──[C5: 10µF]─┼──┤ VDDA (Analog)                    │                        │
│                     │  │                                  │                        │
│   3.3V ──[C6: 10µF]─┼──┤ VDD3P3_RTC                       │                        │
│                     │  │                                  │                        │
│                    GND─┤ GND (multiple pins)              │                        │
│                        │                                  │                        │
│   [C7-C12: 100nF each] │  (6x decoupling caps, one per    │                        │
│   placed very close    │   power pin pair)                │                        │
│   to VDD pins          │                                  │                        │
│                        │                                  │                        │
│   ┌─[R1: 10kΩ]─3.3V   │                                  │                        │
│   │                    │                                  │                        │
│   └────[SW1: Reset]────┤ EN (Enable)                      │                        │
│        │               │                                  │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│   ┌─[R2: 10kΩ]─3.3V   │                                  │                        │
│   │                    │                                  │                        │
│   └────[SW2: Boot]─────┤ GPIO0 (BOOT)                     │                        │
│        │               │                                  │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│   ┌─[R3: 10kΩ]─3.3V   │                                  │                        │
│   │                    │                                  │                        │
│   └────[SW3: Debug]────┤ GPIO13 (Debug Button)            │                        │
│        │               │                                  │                        │
│   [C13: 100nF]         │                                  │                        │
│        │               │                                  │                        │
│       GND              │                                  │                        │
│                        │                                  │                        │
│                        │  GPIO16 (TX2) ───────────────────┼────► To MAX485 DI      │
│                        │  GPIO17 (RX2) ◄──────────────────┼───── From MAX485 RO    │
│                        │  GPIO4  (DE/RE) ──────────────────┼────► To MAX485 DE+RE   │
│                        │                                  │      (held LOW)        │
│                        │                                  │                        │
│                        │  GPIO25 (TX1) ───────────────────┼────► External Serial TX│
│                        │  GPIO26 (RX1) ◄──────────────────┼───── External Serial RX│
│                        │                                  │                        │
│                        │  GPIO2  ──────────────────────────┼────► Status LED driver │
│                        │  GPIO15 ──────────────────────────┼────► Debug LED driver  │
│                        │                                  │                        │
│                        └──────────────────────────────────┘                        │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          RS-485 INTERFACE SECTION                                   │
│                                                                                     │
│                        ┌──────────────────────────────────┐                        │
│                        │         MAX485                   │                        │
│                        │                                  │                        │
│   3.3V ────────────────┤ VCC                              │                        │
│                        │                                  │                        │
│   GND ─────────────────┤ GND                              │                        │
│                        │                                  │                        │
│   From ESP32 GPIO16 ───┤ DI (Data In)                     │                        │
│                        │                                  │                        │
│   To ESP32 GPIO17 ◄────┤ RO (Receiver Out)                │                        │
│                        │                                  │                        │
│   From ESP32 GPIO4 ────┤ DE (Driver Enable)               │                        │
│                        │                                  │                        │
│   From ESP32 GPIO4 ────┤ RE (Receiver Enable)             │                        │
│                        │                                  │                        │
│                        │                              A ──┼───┬──[D3: TVS]──GND    │
│                        │                                  │   │                    │
│                        │                              B ──┼───┼──[D4: TVS]──GND    │
│                        │                                  │   │                    │
│                        └──────────────────────────────────┘   │                    │
│                                                               │                    │
│                                                          [C14: 100nF]              │
│                                                               │                    │
│                                                              GND                   │
│                                                               │                    │
│                                                               │                    │
│   To Orion Bus ◄──────────────────────────────────────────────┴─── A              │
│                                                                                    │
│   To Orion Bus ◄──────────────────────────────────────────────┬─── B              │
│                                                               │                    │
│                                                          [R4: 120Ω]               │
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
│   STATUS LED (Green) - GPIO2                                                        │
│                                                                                     │
│   From ESP32 GPIO2 ──[R5: 220Ω]───┬──┤ Q1: 2N3904                                  │
│                                   │    NPN Transistor                              │
│                                  GND   │                                            │
│                                       │                                             │
│                                       ├─── LED1 Cathode (-)                         │
│                                       │                                             │
│                                  [R6: 1kΩ]                                          │
│                                       │                                             │
│                                   LED1 Anode (+)                                    │
│                                   (Green)                                           │
│                                       │                                             │
│                                     3.3V                                            │
│                                                                                     │
│   DEBUG LED (Red) - GPIO15                                                          │
│                                                                                     │
│   From ESP32 GPIO15 ──[R7: 220Ω]───┬──┤ Q2: 2N3904                                 │
│                                    │    NPN Transistor                             │
│                                   GND   │                                           │
│                                        │                                            │
│                                        ├─── LED2 Cathode (-)                        │
│                                        │                                            │
│                                   [R8: 1kΩ]                                         │
│                                        │                                            │
│                                    LED2 Anode (+)                                   │
│                                    (Red)                                            │
│                                        │                                            │
│                                      3.3V                                           │
│                                                                                     │
│   POWER LED (Yellow) - Always On                                                    │
│                                                                                     │
│   3.3V ──[R9: 1kΩ]── LED3 Anode (+) ── LED3 Cathode (-) ── GND                     │
│                      (Yellow)                                                       │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          EXTERNAL CONNECTIONS                                       │
│                                                                                     │
│   SERIAL CABLE (to external system)                                                 │
│   ┌──────────────────────────────────────┐                                         │
│   │  J1: 3-pin screw terminal            │                                         │
│   │                                      │                                         │
│   │  Pin 1: GND ◄────────────────────────┼──── Common Ground                       │
│   │                                      │                                         │
│   │  Pin 2: TX (from ESP32 GPIO25) ◄─────┼──── To external system RX              │
│   │                                      │                                         │
│   │  Pin 3: RX (to ESP32 GPIO26) ────────┼──── From external system TX            │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   RS-485 BUS (to Orion system)                                                      │
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
│   │  Pin 1: +12V ◄───────────────────────┼──── 12V supply                          │
│   │                                      │                                         │
│   │  Pin 2: GND ◄────────────────────────┼──── Ground                              │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
│   USB PROGRAMMING (optional, for firmware updates)                                  │
│   ┌──────────────────────────────────────┐                                         │
│   │  J4: USB Micro-B connector           │                                         │
│   │                                      │                                         │
│   │  Connected to ESP32 USB pins         │                                         │
│   │  (if using bare ESP32 module)        │                                         │
│   │                                      │                                         │
│   └──────────────────────────────────────┘                                         │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Component Reference

### Power Supply Components

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| F1 | 500mA | 0ZCJ0050FF2E | 1812 SMD | Resettable PTC fuse |
| D1 | 1N5819 | 1N5819 | DO-41 | Schottky diode, reverse polarity protection |
| D2 | 13V | SMBJ13A | SMB | TVS diode, overvoltage protection |
| U1 | LM2596S | LM2596S-3.3 | Module | Buck converter, 12V→3.3V, 3A |
| C1 | 100µF | ECA-1EM101 | Radial | Input bulk capacitor, 25V |
| C2 | 100µF | ECA-1EM101 | Radial | Output bulk capacitor, 16V |
| C3 | 10µF | CL21A106KAYNNNE | 0805 | Output filter, X7R ceramic |

### ESP32 and Decoupling

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U2 | ESP32 | ESP32-WROOM-32D | Module | Main microcontroller |
| C4-C6 | 10µF | CL21A106KAYNNNE | 0805 | Power supply decoupling, X7R |
| C7-C12 | 100nF | GRM188R71H104KA93D | 0603 | High-freq decoupling, X7R |
| R1 | 10kΩ | RC0805FR-0710KL | 0805 | EN pull-up |
| R2 | 10kΩ | RC0805FR-0710KL | 0805 | GPIO0 pull-up |
| R3 | 10kΩ | RC0805FR-0710KL | 0805 | GPIO13 pull-up |
| SW1 | Reset | B3F-1000 | 6x6mm | Reset button |
| SW2 | Boot | B3F-1000 | 6x6mm | Boot/program button |
| SW3 | Debug | B3F-1000 | 6x6mm | **Debug button (separate!)** |
| C13 | 100nF | GRM188R71H104KA93D | 0603 | Debug button debounce |

### RS-485 Interface

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| U3 | MAX485 | MAX485CSA+ | SOIC-8 | RS-485 transceiver |
| D3, D4 | TVS | PESD1CAN | SOT-23 | ESD protection on A, B lines |
| C14 | 100nF | GRM188R71H104KA93D | 0603 | Noise filtering across A-B |
| R4 | 120Ω | RC0805FR-07120RL | 0805 | Optional termination resistor |

### LED Drivers

| Ref | Value | Part Number | Package | Notes |
|-----|-------|-------------|---------|-------|
| Q1, Q2 | 2N3904 | 2N3904 | TO-92 | NPN transistor, LED driver |
| R5, R7 | 220Ω | RC0805FR-07220RL | 0805 | Transistor base resistor |
| R6, R8 | 1kΩ | RC0805FR-071KL | 0805 | LED current limit resistor |
| R9 | 1kΩ | RC0805FR-071KL | 0805 | Power LED resistor |
| LED1 | Green | L-53GD | 3mm THT | Status indicator |
| LED2 | Red | L-53ID | 3mm THT | Debug mode indicator |
| LED3 | Yellow | L-53YD | 3mm THT | Power indicator |

### Connectors

| Ref | Type | Part Number | Notes |
|-----|------|-------------|-------|
| J1 | 3-pin | Phoenix 1757019 | External serial cable |
| J2 | 2-pin | Phoenix 1757019 | RS-485 bus |
| J3 | 2-pin | Phoenix 1757019 | Power input |
| J4 | USB Micro-B | Molex 47589-0001 | Programming (optional) |

---

## Key Design Improvements

### 1. Separate Debug Button (GPIO13 instead of GPIO0)

**Why this is better:**

| Feature | BOOT Button (GPIO0) | Separate Button (GPIO13) |
|---------|---------------------|--------------------------|
| **Programming mode** | Used for flash mode | Not used, safe |
| **Boot behavior** | Must be HIGH at boot | No restrictions |
| **Conflicts** | Can interfere with boot | No conflicts |
| **Professional** | Shared function | Dedicated function |

**GPIO13 is ideal because:**
- Not a strapping pin (no boot mode conflicts)
- Not used by flash, JTAG, or other critical functions
- Safe to pull high or low at any time
- Can use internal pull-up

### 2. Transistor-Driven LEDs

**Why use transistors:**

```
Direct GPIO drive:          Transistor drive:
ESP32 GPIO ──[Resistor]── LED    ESP32 GPIO ──[220Ω]──┤ NPN ├── LED
Max current: 12mA               Max current: 30mA+
GPIO stress: High               GPIO stress: Low
Brightness: Dim                 Brightness: Bright
```

**Benefits:**
- ✅ Brighter LEDs (20-30mA vs 12mA)
- ✅ Protects ESP32 GPIO pins
- ✅ Better current control
- ✅ Can drive high-power LEDs if needed

### 3. Proper Power Supply Design

**Buck converter vs Linear regulator:**

| Parameter | Linear (LM1117) | Buck (LM2596S) |
|-----------|-----------------|----------------|
| **Efficiency** | ~30% (wastes 70% as heat!) | ~92% (minimal heat) |
| **Heat dissipation** | (12V-3.3V) × 270mA = **2.3W** | ~0.2W |
| **Size** | Small IC, large heatsink | Larger module |
| **Cost** | $0.20 | $1.20 |
| **Best for** | Low current (<100mA) | High current (>100mA) |

**For this project:** Buck converter is essential because:
- ESP32 with WiFi draws 160-260mA peak
- Linear regulator would overheat (2.3W dissipation!)
- Buck converter stays cool, efficient

### 4. Protection Circuits

**Three layers of protection:**

```
12V Input ──[Fuse]──[Schottky]──[TVS]──[Buck]── 3.3V
             │         │          │
          Overcurrent  Reverse   Overvoltage
          protection   polarity  protection
```

| Protection | Component | Protects Against |
|------------|-----------|------------------|
| **Fuse** | 500mA PTC | Short circuits, overcurrent |
| **Schottky diode** | 1N5819 | Reverse polarity (wrong wiring) |
| **TVS diode** | SMBJ13A | Voltage spikes (inductive loads) |

**Why each is needed:**
- **Fuse**: If output shorts, fuse opens (resets when cool)
- **Schottky**: If 12V connected backwards, diode blocks (low voltage drop)
- **TVS**: If voltage spike >13V, TVS clamps to safe level

### 5. Decoupling Strategy

**Every power pin needs decoupling:**

```
VDD pin ──┬── 10µF (bulk storage)
          │
          └── 100nF (high-freq noise)
          │
         GND
```

**Why both values:**
- **10µF**: Handles current surges (WiFi TX bursts)
- **100nF**: Filters high-frequency noise (switching noise)

**Placement is critical:**
- Place capacitors **as close as possible** to ESP32 pins (<5mm)
- Use short, wide traces
- Ground via right next to cap

### 6. RS-485 Protection

**ESD protection on A and B lines:**

```
A line ──[TVS diode]── GND
B line ──[TVS diode]── GND
```

**Why needed:**
- RS-485 cables can be long (hundreds of meters)
- Static discharge from touching cables
- Lightning-induced surges
- TVS diodes clamp voltage to safe levels

---

## PCB Layout Guidelines

### Critical Traces

1. **Power traces (3.3V, GND)**
   - Width: 1mm minimum
   - Keep short and direct
   - Use ground plane on bottom layer

2. **RS-485 differential pair (A, B)**
   - Width: 0.3mm
   - Spacing: 0.3mm (for 120Ω differential impedance)
   - Keep equal length (±1mm)
   - Route together, avoid vias if possible

3. **High-speed signals (SPI flash)**
   - Width: 0.2mm
   - Keep short (<20mm)
   - Ground plane underneath

### Component Placement

```
┌─────────────────────────────────────────┐
│  [Power Section]                        │
│  Fuse → Diode → Buck → Caps             │
│                                         │
│  [ESP32 Module]        [RS-485]         │
│  Decoupling caps       MAX485           │
│  very close!           Protection       │
│                                         │
│  [LEDs]                [Buttons]        │
│  Q1, Q2, Q3            SW1, SW2, SW3    │
│                                         │
│  [Connectors on edges]                  │
│  J1, J2, J3, J4                         │
└─────────────────────────────────────────┘
```

### Ground Plane

- **Bottom layer**: Solid copper pour (GND)
- **Top layer**: Minimal GND traces, use vias to bottom plane
- **Via stitching**: Every 5mm around perimeter
- **Thermal reliefs**: For hand soldering pads

---

## Assembly Notes

### Soldering Order

1. **SMD components first** (smallest to largest)
   - Resistors, capacitors (0603, 0805)
   - ICs (SOIC-8)
   - Modules (if SMD)

2. **Through-hole components**
   - Transistors (TO-92)
   - LEDs (3mm)
   - Buttons
   - Screw terminals

3. **Modules last**
   - ESP32 module
   - Buck converter module
   - MAX485 module (if using breakout)

### Testing Procedure

1. **Visual inspection**
   - No solder bridges
   - All components oriented correctly
   - No cold solder joints

2. **Continuity test**
   - GND plane continuous
   - No shorts between power rails
   - All connections as per schematic

3. **Power-on test (no ESP32 yet)**
   - Apply 12V to J3
   - Measure 3.3V at buck converter output
   - Check for excessive heat

4. **ESP32 installation**
   - Solder ESP32 module
   - Apply power
   - Measure 3.3V at ESP32 VDD pins

5. **Firmware flash**
   - Connect USB (or use external programmer)
   - Flash test firmware
   - Verify serial output

6. **Full functional test**
   - Connect to RS-485 bus
   - Verify packet reception
   - Test all LEDs
   - Test debug button
   - Test WiFi connection

---

## Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| No 3.3V output | Buck converter not working | Check 12V input, check fuse |
| ESP32 won't boot | Insufficient decoupling | Add more caps, check solder joints |
| No RS-485 reception | Wrong A/B wiring | Swap A and B |
| LEDs too dim | Wrong resistor values | Use lower value (e.g., 470Ω) |
| LEDs don't light | Transistor not switching | Check base resistor, GPIO output |
| Debug button not working | Wrong GPIO or no pull-up | Verify GPIO13, check R3 |
| WiFi won't connect | Antenna issue | Check ESP32 module antenna |
| High CRC errors | Noise on RS-485 | Add ferrite beads, check grounding |

---

## Comparison: Old vs New Design

| Feature | Old Design (Basic) | New Design (Optimized) |
|---------|-------------------|------------------------|
| **Power supply** | Linear regulator (hot!) | Buck converter (efficient) |
| **Protection** | None | Fuse + diodes + TVS |
| **LED drive** | Direct GPIO | Transistor-driven |
| **Debug button** | Shared BOOT button | Separate GPIO13 |
| **Decoupling** | Minimal | Proper (10µF + 100nF per pin) |
| **RS-485 protection** | None | ESD diodes |
| **Efficiency** | ~30% | ~92% |
| **Heat dissipation** | 2.3W (needs heatsink!) | 0.2W (cool) |
| **Reliability** | Basic | Professional |
| **Cost** | ~$8 | ~$15 |

---

## Next Steps

1. **Review schematic** - Verify all connections
2. **Design PCB** - Use KiCad, Eagle, or EasyEDA
3. **Order components** - See BOM_complete.md
4. **Fabricate PCB** - JLCPCB, PCBWay, or OSH Park
5. **Assemble** - Follow assembly notes
6. **Test** - Follow testing procedure
7. **Deploy** - Install in enclosure, connect to Orion system

---

## Files Included

- `BOM_complete.md` - Full bill of materials with sourcing info
- `esp32_gateway_schematic_optimized.md` - This file (circuit diagram)
- `main.cpp` - Firmware with GPIO13 debug button support

---

## Support

For questions about this schematic:
- Check component datasheets
- Refer to ESP32 hardware design guidelines
- See BOM_complete.md for component specifications
