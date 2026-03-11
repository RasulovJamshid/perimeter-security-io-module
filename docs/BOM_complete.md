# ESP32 Orion Gateway — Complete Bill of Materials (BOM)

## Production-Ready Design with Proper Power Management

This BOM includes all components for a professional, reliable ESP32 gateway with:
- Proper power regulation from 12V Orion system
- Protection circuits (reverse polarity, overvoltage, ESD)
- Transistor-driven LEDs for better current control
- Separate debug button (not BOOT button)
- Filtering and decoupling for noise immunity

---

## Core Components

| Item | Quantity | Part Number / Specs | Price (USD) | Notes |
|------|----------|---------------------|-------------|-------|
| **Microcontroller** |
| ESP32-WROOM-32 Module | 1 | ESP32-WROOM-32D/32U | $3.00 | 4MB flash, WiFi+BT |
| OR ESP32 DevKit v1 | 1 | ESP32-DevKitC-32D | $4.00 | Pre-assembled dev board (easier) |
| **RS-485 Transceiver** |
| MAX485 IC | 1 | MAX485CSA+ (SOIC-8) | $1.50 | 3.3V compatible |
| OR MAX485 Module | 1 | MAX485 breakout board | $0.80 | Pre-assembled (easier) |
| **Power Supply** |
| DC-DC Buck Converter | 1 | LM2596S-3.3 module | $1.20 | 12V→3.3V, 3A output |
| OR Buck Converter IC | 1 | MP1584EN (SOIC-8) | $0.60 | For custom PCB |
| **Protection Components** |
| Schottky Diode (reverse polarity) | 1 | 1N5819 (DO-41) | $0.10 | 1A, 40V |
| TVS Diode (overvoltage protection) | 1 | SMBJ13A (SMB) | $0.30 | 13V, 600W transient |
| Fuse (resettable) | 1 | 0ZCJ0050FF2E (1812) | $0.40 | 500mA hold, 1A trip |
| **Voltage Regulation (if using bare ESP32)** |
| LDO Regulator | 1 | AMS1117-3.3 (SOT-223) | $0.20 | 1A, low dropout |
| **Capacitors** |
| Electrolytic 100µF 25V | 2 | Panasonic ECA-1EM101 | $0.20 | Input/output filtering |
| Ceramic 10µF 16V (X7R) | 4 | Samsung CL21A106KAYNNNE | $0.40 | Decoupling |
| Ceramic 100nF 50V (X7R) | 6 | Murata GRM188R71H104KA93D | $0.30 | High-freq decoupling |
| Ceramic 22pF 50V (C0G) | 2 | TDK C1608C0G1H220J | $0.10 | Crystal load caps |
| **Resistors (0805 SMD or 1/4W THT)** |
| 10kΩ (pull-up) | 4 | Yageo RC0805FR-0710KL | $0.20 | Button, EN, BOOT |
| 220Ω (LED current limit) | 3 | Yageo RC0805FR-07220RL | $0.15 | For transistor base |
| 1kΩ (LED series) | 3 | Yageo RC0805FR-071KL | $0.15 | LED current limit |
| 120Ω (RS-485 termination) | 1 | Yageo RC0805FR-07120RL | $0.05 | Optional, if needed |
| **Transistors** |
| NPN Transistor (LED driver) | 3 | 2N3904 (TO-92) | $0.15 | For LED switching |
| OR BC547B (TO-92) | 3 | BC547B | $0.12 | Alternative |
| **LEDs** |
| LED Green 3mm | 1 | Kingbright L-53GD | $0.10 | Status indicator |
| LED Red 3mm | 1 | Kingbright L-53ID | $0.10 | Debug mode |
| LED Yellow 3mm | 1 | Kingbright L-53YD | $0.10 | Power indicator |
| **Buttons** |
| Tactile Switch (debug) | 1 | Omron B3F-1000 | $0.30 | 6x6mm, SPST-NO |
| Tactile Switch (reset) | 1 | Omron B3F-1000 | $0.30 | Optional |
| **Connectors** |
| Screw Terminal 2-pin 5mm | 2 | Phoenix 1757019 | $0.60 | RS-485 A/B, Power in |
| Pin Header 3-pin 2.54mm | 1 | Amphenol 10129378-903001BLF | $0.10 | Serial cable out |
| USB Micro-B Connector | 1 | Molex 47589-0001 | $0.40 | Programming (if bare ESP32) |
| **Miscellaneous** |
| Crystal 40MHz | 1 | Abracon ABM8G-40.000MHZ-4Y-T3 | $0.50 | If using bare ESP32 |
| PCB (custom) | 1 | 80x50mm, 2-layer | $2.00 | Or use breadboard/perfboard |
| Enclosure (optional) | 1 | Hammond 1591XXSSBK | $3.00 | 85x56x25mm plastic box |

---

## Assembly Options

### Option A: Development Board (Easiest)
**Total Cost: ~$8-10**

Use pre-made modules:
- ESP32 DevKit v1 ($4)
- MAX485 module ($0.80)
- LM2596S buck converter module ($1.20), mp2315
- Breadboard + jumper wires ($2)
- LEDs, resistors, buttons ($1)

**Pros**: No soldering, quick assembly, easy debugging
**Cons**: Larger size, less reliable connections

### Option B: Custom PCB (Professional)
**Total Cost: ~$15-20** (including PCB fabrication)

Use discrete components on custom PCB:
- All SMD components
- Compact design (80x50mm)
- Screw terminals for all connections
- Mounting holes

**Pros**: Compact, reliable, professional
**Cons**: Requires PCB design, SMD soldering skills

### Option C: Perfboard Assembly (Middle Ground)
**Total Cost: ~$10-12**

Through-hole components on perfboard:
- ESP32 module on breakout board
- Through-hole resistors, capacitors
- DIP-8 MAX485 IC
- Hand-wired connections

**Pros**: Moderate cost, repairable, no PCB needed
**Cons**: Larger than custom PCB, more assembly time

---

## Detailed Component Breakdown

### 1. Power Supply Section

**Input: 12V from Orion System PSU**

```
12V ──[Fuse]──[Schottky Diode]──[TVS Diode]──[100µF]──[Buck Converter]──[100µF]──[10µF]── 3.3V
                    │                │                                        │
                   GND              GND                                      GND
```

| Component | Purpose | Specs |
|-----------|---------|-------|
| Resettable Fuse | Overcurrent protection | 500mA hold, 1A trip |
| Schottky Diode | Reverse polarity protection | 1N5819, 1A, 40V |
| TVS Diode | Transient voltage suppression | SMBJ13A, 13V, 600W |
| Buck Converter | 12V → 3.3V regulation | LM2596S, 3A, 92% efficiency |
| 100µF Electrolytic | Input/output bulk filtering | 25V, low ESR |
| 10µF Ceramic | Output high-freq filtering | X7R, 16V |

**Why this design:**
- **Fuse**: Protects against short circuits
- **Schottky diode**: Prevents damage if 12V is connected backwards (low voltage drop)
- **TVS diode**: Clamps voltage spikes from inductive loads on Orion bus
- **Buck converter**: Efficient 12V→3.3V conversion (better than linear regulator)
- **Capacitors**: Smooth out ripple and noise

### 2. ESP32 Power Decoupling

**Every power pin needs decoupling:**

| Location | Capacitor | Purpose |
|----------|-----------|---------|
| VDD pins (x3) | 10µF + 100nF each | Bulk + high-freq decoupling |
| VDDA (analog) | 10µF + 100nF | Analog supply filtering |
| VDD3P3_RTC | 10µF + 100nF | RTC supply |

**Total: 4x 10µF + 6x 100nF**

### 3. RS-485 Interface

**MAX485 Connection:**

| MAX485 Pin | Connects To | Notes |
|------------|-------------|-------|
| VCC | 3.3V | Power supply |
| GND | GND | Ground |
| DI (Data In) | ESP32 GPIO16 (TX2) | Not used in passive mode |
| RO (Receiver Out) | ESP32 GPIO17 (RX2) | Bus data to ESP32 |
| DE (Driver Enable) | ESP32 GPIO4 | Held LOW (receive only) |
| RE (Receiver Enable) | ESP32 GPIO4 | Tied to DE, held LOW |
| A | RS-485 Bus A | Screw terminal |
| B | RS-485 Bus B | Screw terminal |

**Protection:**
- 100nF ceramic cap across A-B (noise filtering)
- Optional 120Ω termination resistor (only if at bus end)
- TVS diodes on A and B for ESD protection (optional but recommended)

### 4. LED Driver Circuit (Proper Design)

**Why use transistors instead of direct GPIO drive:**
- ESP32 GPIO max current: 12mA (not enough for bright LED)
- With transistor: Can drive 20-30mA safely
- Better current control
- Protects ESP32 GPIO pins

**Circuit per LED:**

```
ESP32 GPIO ──[220Ω]──┬──┤ NPN
                     │    Transistor
                    GND   (2N3904)
                          │
                          ├── LED Cathode
                          │
                     [1kΩ]
                          │
                      LED Anode
                          │
                        3.3V
```

**Component values:**
- Base resistor (220Ω): Limits base current to ~15mA
- LED series resistor (1kΩ): Limits LED current to ~2mA (visible but not too bright)
- Transistor: 2N3904 or BC547 (common NPN, hFE ~100-200)

**LED assignments:**
| LED Color | GPIO | Purpose |
|-----------|------|---------|
| Green | GPIO2 | Status (bus activity, alarms) |
| Red | GPIO15 | Debug mode active |
| Yellow | Power rail | Power indicator (always on) |

### 5. Debug Button Circuit (Separate from BOOT)

**Proper button design:**

```
3.3V ──[10kΩ]──┬── ESP32 GPIO13
               │
           [Button]
               │
              GND
```

**Plus 100nF capacitor across button for debouncing:**

```
Button ──[100nF]── GND
```

**Why GPIO13:**
- Not used by BOOT/EN/STRAP pins
- Safe to use as input with pull-up
- No conflicts with flash mode

**Why separate button:**
- BOOT button (GPIO0) is used for programming
- Separate button allows debug toggle without affecting flash mode
- More professional design

### 6. Reset Button (Optional but Recommended)

```
3.3V ──[10kΩ]──┬── ESP32 EN pin
               │
           [Button]
               │
              GND
```

Plus 100nF cap for debouncing.

---

## Pin Assignment Summary

| ESP32 GPIO | Function | Connects To |
|------------|----------|-------------|
| **RS-485 Bus (UART2)** |
| GPIO16 (TX2) | RS-485 TX (unused) | MAX485 DI |
| GPIO17 (RX2) | RS-485 RX | MAX485 RO |
| GPIO4 | RS-485 DE/RE | MAX485 DE+RE (held LOW) |
| **External Serial (UART1)** |
| GPIO25 (TX1) | Serial TX | External system RX |
| GPIO26 (RX1) | Serial RX | External system TX |
| **LEDs (via transistors)** |
| GPIO2 | Status LED | NPN base (green LED) |
| GPIO15 | Debug LED | NPN base (red LED) |
| **Buttons** |
| GPIO13 | Debug button | Tactile switch to GND |
| EN | Reset button | Tactile switch to GND (optional) |
| **Power** |
| 3V3 | Power input | Buck converter output |
| GND | Ground | Common ground |

---

## Power Budget

| Component | Current (mA) | Notes |
|-----------|--------------|-------|
| ESP32 (idle) | 80 | WiFi off |
| ESP32 (WiFi active) | 160-260 | Peak during TX |
| MAX485 | 0.5 | Receive mode |
| LEDs (3x @ 2mA each) | 6 | With transistor drivers |
| **Total (WiFi off)** | **~90mA** | Serial-only mode |
| **Total (WiFi on)** | **~270mA** | Peak with WiFi TX |

**Buck converter rating**: 3A (plenty of headroom)

**12V input current**: ~30mA typical, ~100mA peak (at 90% efficiency)

---

## PCB Design Recommendations

### Layer Stack (2-layer PCB)
- **Top layer**: Signal traces, components
- **Bottom layer**: Ground plane (maximum coverage)

### Trace Widths
- **Power (3.3V, GND)**: 0.5mm minimum (1mm preferred)
- **RS-485 (A, B)**: 0.3mm, differential pair, 120Ω impedance
- **Signal traces**: 0.2mm minimum

### Component Placement
1. **Power section** (left side): Fuse → Diode → Buck converter → Caps
2. **ESP32** (center): With decoupling caps very close to pins
3. **RS-485** (right side): MAX485 + termination + protection
4. **Connectors** (edges): Screw terminals, pin headers
5. **LEDs/Buttons** (top edge): User interface elements

### Ground Plane
- Solid ground plane on bottom layer
- Thermal reliefs for hand soldering
- Via stitching around perimeter (every 5mm)

---

## Assembly Instructions

### Step 1: Power Supply
1. Solder fuse, diodes, buck converter
2. Add input/output capacitors
3. Test: Apply 12V, verify 3.3V output with multimeter

### Step 2: ESP32 Module
1. Solder ESP32 module or socket
2. Add all decoupling capacitors (very close to pins!)
3. Add pull-up resistors on EN, GPIO0

### Step 3: RS-485 Interface
1. Solder MAX485 IC or module
2. Add protection components
3. Wire to ESP32 GPIOs

### Step 4: LEDs and Buttons
1. Solder transistors
2. Add base and LED resistors
3. Solder LEDs (watch polarity!)
4. Add buttons with pull-ups

### Step 5: Connectors
1. Solder screw terminals
2. Add pin headers
3. Label all connections

### Step 6: Testing
1. Visual inspection (no shorts, no cold joints)
2. Continuity test (GND plane, power rails)
3. Power-on test (measure voltages)
4. Flash firmware via USB
5. Test serial output
6. Test RS-485 reception

---

## Sourcing Components

### Recommended Suppliers

| Supplier | Best For | Shipping |
|----------|----------|----------|
| **Digi-Key** | Professional components, datasheets | Worldwide |
| **Mouser** | Wide selection, good stock | Worldwide |
| **LCSC** | Low-cost SMD components | China, cheap shipping |
| **AliExpress** | Modules, dev boards, cheap parts | China, slow shipping |
| **Amazon** | Quick delivery, kits | Local, fast |

### Component Kits (for prototyping)

| Kit | Contents | Price |
|-----|----------|-------|
| ESP32 DevKit | ESP32 board + USB cable | $5-8 |
| Resistor kit | 0805 SMD, 1/4W THT, common values | $10 |
| Capacitor kit | Ceramic + electrolytic assortment | $15 |
| LED kit | 3mm/5mm, various colors | $5 |
| Transistor kit | 2N3904, BC547, etc. | $8 |

---

## Cost Summary

### Minimal Build (Breadboard Prototype)
- ESP32 DevKit: $4
- MAX485 module: $0.80
- Buck converter module: $1.20
- LEDs, resistors, buttons: $2
- Breadboard + wires: $3
- **Total: ~$11**

### Professional Build (Custom PCB)
- Components (all discrete): $12
- PCB fabrication (5 pcs): $10
- Assembly time: 2-3 hours
- **Total: ~$22 (or $4.40 per unit for 5 boards)**

### Production Build (100 units)
- Components (bulk pricing): $8 per unit
- PCB (100 pcs): $0.50 per unit
- Assembly (pick-and-place): $2 per unit
- **Total: ~$10.50 per unit**

---

## Testing Equipment Needed

| Equipment | Purpose | Cost |
|-----------|---------|------|
| Multimeter | Voltage, continuity testing | $15-50 |
| USB-UART adapter | Serial monitoring | $3-8 |
| Oscilloscope (optional) | RS-485 signal analysis | $50-500 |
| Logic analyzer (optional) | Protocol debugging | $10-100 |

---

## Safety and Compliance

### Electrical Safety
- ✅ Reverse polarity protection (diode)
- ✅ Overcurrent protection (fuse)
- ✅ Overvoltage protection (TVS diode)
- ✅ Proper grounding
- ✅ Isolation from mains voltage

### EMC Considerations
- Ground plane for noise immunity
- Decoupling capacitors on all power pins
- Ferrite beads on RS-485 lines (optional)
- Shielded cable for RS-485 (recommended)

### Certifications (if selling commercially)
- CE marking (Europe)
- FCC Part 15 (USA)
- RoHS compliance (lead-free)

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-10 | Initial BOM with optimized design |

---

## Notes

1. **All capacitors**: Use X7R or X5R dielectric for ceramics (stable over temperature)
2. **All resistors**: 1% tolerance preferred for LED current matching
3. **PCB**: Use ENIG finish for better solderability and corrosion resistance
4. **Enclosure**: IP54 rated if used in dusty environments
5. **Connectors**: Use strain relief on all cables

## Support

For questions or issues with this BOM, refer to:
- ESP32 datasheet: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf
- MAX485 datasheet: https://datasheets.maximintegrated.com/en/ds/MAX1487-MAX491.pdf
- LM2596 datasheet: https://www.ti.com/lit/ds/symlink/lm2596.pdf
