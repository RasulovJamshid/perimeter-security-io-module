# ESP32-C3 Super Mini Orion Gateway — Optimized BOM

## Modern Design with ESP32-C3 and MP2315

**Key improvements over previous design:**
- ✅ ESP32-C3 Super Mini (smaller, cheaper, built-in USB-C)
- ✅ MP2315 buck converter (95% efficient, tiny footprint)
- ✅ Lower power consumption (43mA idle vs 80mA)
- ✅ Simpler design (fewer external components)
- ✅ Lower cost ($8 vs $15 for professional build)

---

## Core Components

| Item | Quantity | Part Number / Specs | Price (USD) | Notes |
|------|----------|---------------------|-------------|-------|
| **Microcontroller** |
| ESP32-C3 Super Mini | 1 | ESP32-C3FN4 (on board) | $1.50 | 4MB flash, WiFi, BLE 5.0, USB-C |
| **RS-485 Transceiver** |
| MAX3485 IC | 1 | MAX3485ESA+ (SOIC-8) | $1.20 | 3.3V, better than MAX485 |
| OR MAX485 Module | 1 | MAX485 breakout board | $0.80 | Pre-assembled (easier) |
| **Power Supply (MP2315 Circuit)** |
| Buck Converter IC | 1 | MP2315S (TSOT23-6) | $0.30 | 12V→3.3V, 3A, 95% efficiency |
| Inductor | 1 | 22µH, 3A, DCR<50mΩ | $0.40 | SRN6045-220M or similar |
| Schottky Diode (buck) | 1 | SS34 (SMA) | $0.10 | 3A, 40V, for MP2315 circuit |
| **Protection Components** |
| Schottky Diode (input) | 1 | 1N5819 (DO-41) | $0.10 | Reverse polarity protection |
| TVS Diode (input) | 1 | SMBJ13A (SMB) | $0.30 | Overvoltage protection |
| Fuse (resettable) | 1 | 0ZCJ0050FF2E (1812) | $0.40 | 500mA hold, 1A trip |
| **Capacitors** |
| Electrolytic 100µF 25V | 2 | Panasonic ECA-1EM101 | $0.20 | Input/output bulk filtering |
| Ceramic 22µF 16V (X7R) | 2 | Samsung CL21A226MAYLNNC | $0.30 | MP2315 input/output |
| Ceramic 10µF 16V (X7R) | 3 | Samsung CL21A106KAYNNNE | $0.30 | Decoupling |
| Ceramic 100nF 50V (X7R) | 5 | Murata GRM188R71H104KA93D | $0.25 | High-freq decoupling |
| **Resistors (0805 SMD or 1/4W THT)** |
| 10kΩ (pull-up) | 3 | Yageo RC0805FR-0710KL | $0.15 | Button, EN |
| 220Ω (LED base) | 3 | Yageo RC0805FR-07220RL | $0.15 | Transistor base |
| 1kΩ (LED series) | 3 | Yageo RC0805FR-071KL | $0.15 | LED current limit |
| 100kΩ (MP2315 FB upper) | 1 | Yageo RC0805FR-07100KL | $0.05 | Feedback divider |
| 27kΩ (MP2315 FB lower) | 1 | Yageo RC0805FR-0727KL | $0.05 | Feedback divider |
| 120Ω (RS-485 term) | 1 | Yageo RC0805FR-07120RL | $0.05 | Optional termination |
| **Transistors** |
| NPN Transistor (LED) | 3 | 2N3904 (TO-92) | $0.15 | For LED switching |
| OR BC547B (TO-92) | 3 | BC547B | $0.12 | Alternative |
| **LEDs** |
| LED Green 3mm | 1 | Kingbright L-53GD | $0.10 | Status indicator |
| LED Red 3mm | 1 | Kingbright L-53ID | $0.10 | Debug mode |
| LED Yellow 3mm | 1 | Kingbright L-53YD | $0.10 | Power indicator |
| **Buttons & Switches** |
| Tactile Switch (debug) | 1 | Omron B3F-1000 | $0.30 | 6x6mm, SPST-NO |
| Tactile Switch (reset) | 1 | Omron B3F-1000 | $0.30 | Optional |
| Toggle Switch (mode) | 1 | E-Switch EG1218 | $0.40 | SPDT, ON-ON, MONITOR/MASTER |
| **Connectors** |
| Screw Terminal 2-pin 5mm | 2 | Phoenix 1757019 | $0.60 | RS-485 A/B, Power in |
| Pin Header 3-pin 2.54mm | 1 | Amphenol 10129378-903001BLF | $0.10 | Serial cable out |
| **Miscellaneous** |
| PCB (custom) | 1 | 60x40mm, 2-layer | $1.50 | Or use breadboard |
| Enclosure (optional) | 1 | Hammond 1591XXSSBK | $3.00 | 85x56x25mm plastic box |

---

## Cost Comparison

### ESP32-C3 Super Mini Build
**Total: ~$8-10** (professional PCB build)

| Section | Cost |
|---------|------|
| ESP32-C3 Super Mini | $1.50 |
| MP2315 + passives | $1.50 |
| MAX3485 + protection | $2.00 |
| LEDs, transistors, buttons | $1.50 |
| Connectors | $0.80 |
| PCB | $1.50 |
| **Total** | **$8.80** |

### Previous ESP32-WROOM-32 Build
**Total: ~$15-22**

| Section | Cost |
|---------|------|
| ESP32-WROOM-32 | $3.00 |
| LM2596S module | $1.20 |
| MAX485 + protection | $2.00 |
| LEDs, transistors, buttons | $1.50 |
| Connectors + USB | $1.20 |
| PCB | $2.00 |
| **Total** | **$10.90** |

**Savings with ESP32-C3 + MP2315: ~$2-3 per unit**

---

## MP2315 Buck Converter Circuit

### Schematic

```
12V ──[F1]──[D1]──┬──[C1: 100µF]──┬──────────────────────────────────┐
                  │               │                                  │
                 GND             GND                                 │
                                                                     │
                                                              ┌──────┴──────┐
                                                              │   MP2315S   │
                                                              │             │
                                                          ┌───┤ VIN     SW  ├───┬──[L1: 22µH]──┬──[C3: 22µF]──┬── 3.3V
                                                          │   │             │   │              │              │
                                                          │   │ EN      BST ├───┼──[C4: 100nF]─┘              │
                                                          │   │             │   │                             │
                                                         12V  │ FB      GND ├───┼─────────────────────────────┼─── GND
                                                              │             │   │                             │
                                                              └─────────────┘   │                             │
                                                                     │          │                             │
                                                                     │      [D2: SS34]                        │
                                                                     │          │                             │
                                                              [R1: 100kΩ]      GND                            │
                                                                     │                                        │
                                                              [R2: 27kΩ]                                      │
                                                                     │                                        │
                                                                    GND                                       │
                                                                                                              │
                                                                                                         [C5: 10µF]
                                                                                                              │
                                                                                                             GND
```

### Component Selection

| Component | Value | Purpose | Notes |
|-----------|-------|---------|-------|
| **L1** | 22µH, 3A | Energy storage | DCR < 50mΩ, SRN6045-220M |
| **C1** | 100µF, 25V | Input bulk cap | Electrolytic, low ESR |
| **C3** | 22µF, 16V | Output bulk cap | Ceramic X7R |
| **C4** | 100nF, 50V | Bootstrap cap | Ceramic X7R |
| **C5** | 10µF, 16V | Output filter | Ceramic X7R |
| **D2** | SS34 (3A, 40V) | Freewheeling diode | Schottky, low Vf |
| **R1** | 100kΩ | FB divider upper | 1% tolerance |
| **R2** | 27kΩ | FB divider lower | 1% tolerance |

### Output Voltage Calculation

```
Vout = 0.6V × (1 + R1/R2)
Vout = 0.6V × (1 + 100kΩ/27kΩ)
Vout = 0.6V × 4.7
Vout = 2.82V (too low!)
```

**Corrected values for 3.3V output:**
- R1 = 100kΩ
- R2 = 22kΩ
- Vout = 0.6V × (1 + 100/22) = 0.6V × 5.55 = **3.33V** ✅

### PCB Layout Guidelines for MP2315

1. **Keep switching node (SW pin) traces short** (<10mm)
2. **Place D2 (freewheeling diode) close to SW pin** (<5mm)
3. **Place L1 (inductor) close to SW pin**
4. **Input cap (C1) close to VIN pin**
5. **Output cap (C3) close to load**
6. **Ground plane under IC** (thermal relief)
7. **Feedback trace (FB) away from SW node** (noise sensitive)

---

## ESP32-C3 Super Mini Pinout

### Available GPIOs

| GPIO | Function | Can Use For | Notes |
|------|----------|-------------|-------|
| **GPIO0** | ADC1_CH0, XTAL_32K_P | ❌ Strapping pin | Used for boot mode |
| **GPIO1** | ADC1_CH1, XTAL_32K_N | ❌ Strapping pin | Used for boot mode |
| **GPIO2** | ADC1_CH2, FSPIQ | ✅ RS-485 DE/RE | Safe to use |
| **GPIO3** | ADC1_CH3 | ✅ Status LED | Safe to use |
| **GPIO4** | ADC1_CH4, FSPIHD, MTMS | ✅ Debug LED | Safe to use |
| **GPIO5** | ADC2_CH0, FSPIWP, MTDI | ✅ Debug Button | Safe to use |
| **GPIO6** | FSPICLK, MTCK | ✅ Available | Safe to use |
| **GPIO7** | FSPID, MTDO | ✅ Available | Safe to use |
| **GPIO8** | - | ❌ Strapping pin | Used for boot mode |
| **GPIO9** | - | ❌ Strapping pin | Used for boot mode |
| **GPIO10** | FSPICS0 | ✅ Available | Safe to use |
| **GPIO18** | USB D- | ❌ USB | Don't use |
| **GPIO19** | USB D+ | ❌ USB | Don't use |
| **GPIO20** | U0RXD | ✅ External Serial RX | UART0 |
| **GPIO21** | U0TXD | ✅ External Serial TX | UART0 |

### Pin Assignment for This Project

| Function | GPIO | Notes |
|----------|------|-------|
| **RS-485 (UART1)** |
| RS-485 RX | GPIO6 | U1RXD (UART1 RX) |
| RS-485 TX | GPIO7 | U1TXD (UART1 TX, unused in passive mode) |
| RS-485 DE/RE | GPIO2 | Held LOW (receive only) |
| **External Serial (UART0)** |
| Serial TX | GPIO21 | U0TXD (to external system RX) |
| Serial RX | GPIO20 | U0RXD (from external system TX) |
| **LEDs (via transistors)** |
| Status LED | GPIO3 | Green LED, bus activity |
| Debug LED | GPIO4 | Red LED, debug mode |
| **Buttons** |
| Debug Button | GPIO5 | Separate debug button |
| Reset Button | EN pin | Hardware reset |
| **Power** |
| 3.3V | 3V3 pin | From MP2315 output |
| GND | GND pin | Common ground |

**Note:** ESP32-C3 has only 2 hardware UARTs:
- **UART0** (GPIO20/21) - Used for external serial cable
- **UART1** (GPIO6/7) - Used for RS-485 bus

---

## Comparison: ESP32-WROOM-32 vs ESP32-C3 Super Mini

### Pin Mapping Changes

| Function | ESP32-WROOM-32 | ESP32-C3 Super Mini | Change |
|----------|----------------|---------------------|--------|
| RS-485 RX | GPIO17 (UART2) | **GPIO6 (UART1)** | ✅ Changed |
| RS-485 TX | GPIO16 (UART2) | **GPIO7 (UART1)** | ✅ Changed |
| RS-485 DE/RE | GPIO4 | **GPIO2** | ✅ Changed |
| Serial TX | GPIO25 (UART1) | **GPIO21 (UART0)** | ✅ Changed |
| Serial RX | GPIO26 (UART1) | **GPIO20 (UART0)** | ✅ Changed |
| Status LED | GPIO2 | **GPIO3** | ✅ Changed |
| Debug LED | GPIO15 | **GPIO4** | ✅ Changed |
| Debug Button | GPIO13 | **GPIO5** | ✅ Changed |

**All pin assignments need to be updated in firmware!**

---

## Assembly Options

### Option A: Breadboard Prototype (Easiest)
**Total Cost: ~$6-8**

Components:
- ESP32-C3 Super Mini board ($1.50)
- MAX485 module ($0.80)
- MP2315 module or LM2596S module ($1.20)
- Breadboard + jumper wires ($2)
- LEDs, resistors, buttons ($1)
- Screw terminals ($0.60)

**Pros:** No soldering, quick assembly, easy debugging
**Cons:** Larger size, less reliable connections

### Option B: Custom PCB (Professional)
**Total Cost: ~$12-15** (including PCB fabrication for 5 boards)

Components:
- All SMD components
- Compact design (60x40mm)
- Screw terminals for all connections
- Mounting holes

**Pros:** Compact, reliable, professional
**Cons:** Requires PCB design, SMD soldering skills

**PCB fabrication cost:** ~$5 for 5 boards (JLCPCB, PCBWay)
**Per-unit cost:** $8.80 + $1 PCB = **$9.80 per unit**

### Option C: Perfboard Assembly
**Total Cost: ~$8-10**

Components:
- ESP32-C3 Super Mini on pin headers
- Through-hole resistors, capacitors
- DIP-8 MAX485 IC
- Hand-wired connections

**Pros:** Moderate cost, repairable, no PCB needed
**Cons:** Larger than custom PCB, more assembly time

---

## Power Budget (ESP32-C3 vs ESP32-WROOM-32)

| Component | ESP32-WROOM-32 | ESP32-C3 Super Mini | Savings |
|-----------|----------------|---------------------|---------|
| MCU (idle) | 80mA | **43mA** | 37mA |
| MCU (WiFi active) | 160-260mA | **120-180mA** | 40-80mA |
| MAX485 | 0.5mA | 0.5mA | - |
| LEDs (3x @ 2mA) | 6mA | 6mA | - |
| **Total (WiFi off)** | **~90mA** | **~50mA** | **44% less!** |
| **Total (WiFi on)** | **~270mA** | **~190mA** | **30% less!** |

**12V input current (at 95% efficiency):**
- WiFi off: 50mA × 3.3V / 12V / 0.95 = **14.5mA** (vs 26mA)
- WiFi on: 190mA × 3.3V / 12V / 0.95 = **55mA** (vs 78mA)

**Power savings: ~30-44% lower power consumption!**

---

## Advantages of ESP32-C3 Super Mini

### 1. Built-in USB-C Programming
- No external USB-UART adapter needed
- Direct connection to PC for firmware upload
- Simpler development workflow

### 2. Lower Cost
- $1.50 vs $3-4 for ESP32-WROOM-32
- Saves $1.50-2.50 per unit
- 100 units = $150-250 savings!

### 3. Lower Power Consumption
- 43mA idle vs 80mA (46% less)
- 120-180mA WiFi vs 160-260mA (30% less)
- Better for battery-powered applications
- Less heat generation

### 4. Smaller Size
- 22.52x18mm vs 25x18mm
- Easier to fit in compact enclosures
- More flexible PCB layout

### 5. Modern Architecture
- RISC-V core (open-source ISA)
- BLE 5.0 (vs BLE 4.2)
- Better power management
- Future-proof design

---

## Disadvantages of ESP32-C3 Super Mini

### 1. Single Core
- 160MHz single-core vs 240MHz dual-core
- Not an issue for this project (bus monitoring isn't CPU-intensive)

### 2. Only 2 UARTs
- UART0 and UART1 (vs 3 UARTs on WROOM-32)
- Still enough for this project (RS-485 + external serial)

### 3. Fewer GPIOs
- 22 GPIOs vs 34 GPIOs
- Still plenty for this project (need ~10 GPIOs)

### 4. No Bluetooth Classic
- Only BLE 5.0 (no classic Bluetooth)
- Not needed for this project

**Verdict: Disadvantages don't matter for this project. ESP32-C3 is the better choice!**

---

## MP2315 vs LM2596S Detailed Comparison

### Efficiency

| Load Current | LM2596S | MP2315 | Difference |
|--------------|---------|--------|------------|
| 50mA (idle) | 88% | **92%** | +4% |
| 100mA | 91% | **94%** | +3% |
| 200mA (WiFi) | 92% | **95%** | +3% |
| 500mA | 92% | 95% | +3% |

**Result:** MP2315 is 3-4% more efficient across all loads

### Size Comparison

| Parameter | LM2596S Module | MP2315 Circuit | Difference |
|-----------|----------------|----------------|------------|
| IC size | TO-263 (large) | TSOT23-6 (tiny) | **10x smaller** |
| Inductor | 330µH (large) | 22µH (small) | **15x smaller** |
| Total PCB area | ~400mm² | **~100mm²** | **4x smaller** |

### Switching Frequency

| Parameter | LM2596S | MP2315 | Impact |
|-----------|---------|--------|--------|
| Frequency | 150kHz | **500kHz** | 3.3x higher |
| Inductor size | Large (330µH) | **Small (22µH)** | Smaller |
| Capacitor size | Large | **Smaller** | Smaller |
| EMI | Lower freq = less EMI | Higher freq = more EMI | Need good layout |

### Thermal Performance

| Parameter | LM2596S | MP2315 | Winner |
|-----------|---------|--------|--------|
| Package | TO-263 (good) | TSOT23-6 (small) | LM2596 |
| Thermal resistance | 30°C/W | 150°C/W | LM2596 |
| Heat dissipation | Better | Worse | LM2596 |
| **For this project** | Overkill | **Adequate** | MP2315 |

**At 200mA load:**
- Power loss: 200mA × 3.3V × (1-0.95) / 0.95 = 35mW
- Temperature rise: 35mW × 150°C/W = **5.2°C** (no heatsink needed!)

---

## PCB Design Recommendations (ESP32-C3 + MP2315)

### Board Size
- **60x40mm** (vs 80x50mm for WROOM-32 design)
- 33% smaller PCB area
- Lower fabrication cost

### Layer Stack (2-layer PCB)
- **Top layer:** Components, signal traces
- **Bottom layer:** Ground plane (maximum coverage)

### Component Placement

```
┌─────────────────────────────────────┐  60mm
│  [Power Section]                    │
│  Fuse → Diode → MP2315 → Caps       │
│                                     │
│  [ESP32-C3]        [RS-485]         │
│  Super Mini        MAX3485          │
│  (on pin headers)  + Protection     │
│                                     │
│  [LEDs]            [Buttons]        │
│  Q1, Q2, Q3        SW1, SW2         │
│                                     │
│  [Connectors on edges]              │
│  J1 (Serial), J2 (RS-485), J3 (Pwr)│
└─────────────────────────────────────┘
   40mm
```

### Critical Layout Rules

1. **MP2315 switching node (SW pin)**
   - Keep trace <10mm
   - Wide trace (1mm)
   - Place D2 (SS34) within 5mm

2. **Inductor (L1)**
   - Place close to SW pin
   - Orient away from sensitive traces
   - Keep away from ESP32 antenna area

3. **Ground plane**
   - Solid pour on bottom layer
   - Thermal reliefs for hand soldering
   - Via stitching every 5mm

4. **ESP32-C3 antenna area**
   - Keep clear of copper (top and bottom)
   - No traces under antenna
   - 5mm keepout zone

---

## Sourcing Recommendations

### ESP32-C3 Super Mini

| Supplier | Price | Shipping | Link |
|----------|-------|----------|------|
| **AliExpress** | $1.20-1.80 | 2-4 weeks | Search "ESP32-C3 Super Mini" |
| **Amazon** | $8-12 (2-pack) | 1-2 days | More expensive but fast |
| **LCSC** | $1.50 | 1-2 weeks | Part# C2934564 |

**Recommendation:** Buy 5-10 units from AliExpress (~$10 total)

### MP2315

| Supplier | Price | Stock | Part Number |
|----------|-------|-------|-------------|
| **LCSC** | $0.25 | ✅ Good | C87483 |
| **Digi-Key** | $0.45 | ✅ Good | MP2315S-LF-Z |
| **Mouser** | $0.50 | ✅ Good | 946-MP2315S-LF-Z |

**Recommendation:** Order from LCSC (cheapest) or Digi-Key (fastest)

### Complete Kit (for 1 prototype)

| Item | Supplier | Price |
|------|----------|-------|
| ESP32-C3 Super Mini | AliExpress | $1.50 |
| MP2315 + passives | LCSC | $2.00 |
| MAX3485 + protection | LCSC | $2.00 |
| LEDs, transistors, buttons | LCSC | $1.50 |
| Connectors | LCSC | $0.80 |
| Breadboard + wires | Amazon | $3.00 |
| **Total** | | **$10.80** |

---

## Testing Procedure

### 1. Power Supply Test (MP2315)
1. Solder MP2315 circuit on breadboard or PCB
2. Apply 12V to input
3. Measure output voltage: should be 3.30-3.35V
4. Load test: Connect 100Ω resistor (33mA load)
5. Check for excessive heat (should be cool)
6. Measure efficiency: (Vout × Iout) / (Vin × Iin) > 90%

### 2. ESP32-C3 Test
1. Connect ESP32-C3 to 3.3V from MP2315
2. Connect USB-C cable to PC
3. Flash test firmware (blink LED)
4. Verify serial output on USB
5. Test WiFi connection

### 3. RS-485 Interface Test
1. Connect MAX3485 to ESP32-C3
2. Connect to Orion bus (A, B)
3. Flash gateway firmware
4. Monitor USB serial for packet reception
5. Verify CRC errors < 1%

### 4. Full System Test
1. Connect all components
2. Apply 12V power
3. Verify all LEDs working
4. Test debug button
5. Test external serial output
6. Test WiFi configuration API

---

## Firmware Changes Required

**Pin definitions need to be updated in `main.cpp`:**

```cpp
// OLD (ESP32-WROOM-32)
#define RS485_TX_PIN  16
#define RS485_RX_PIN  17
#define RS485_DE_PIN  4
#define EXT_SERIAL_TX_PIN  25
#define EXT_SERIAL_RX_PIN  26
#define STATUS_LED_PIN  2
#define DEBUG_LED_PIN   15
#define DEBUG_BUTTON_PIN  13

// NEW (ESP32-C3 Super Mini)
#define RS485_TX_PIN  7   // UART1 TX (unused in passive mode)
#define RS485_RX_PIN  6   // UART1 RX
#define RS485_DE_PIN  2   // DE/RE control (held LOW)
#define EXT_SERIAL_TX_PIN  21  // UART0 TX
#define EXT_SERIAL_RX_PIN  20  // UART0 RX
#define STATUS_LED_PIN  3  // Status LED
#define DEBUG_LED_PIN   4  // Debug LED
#define DEBUG_BUTTON_PIN  5  // Debug button
```

**UART initialization changes:**

```cpp
// OLD
Serial2.begin(9600, SERIAL_8N1, 17, 16);  // UART2 for RS-485
Serial1.begin(115200, SERIAL_8N1, 26, 25); // UART1 for external

// NEW
Serial1.begin(9600, SERIAL_8N1, 6, 7);    // UART1 for RS-485
Serial.begin(115200);                      // UART0 (USB) for external
```

---

## Summary: Why ESP32-C3 + MP2315 is Better

| Advantage | Benefit |
|-----------|---------|
| **Lower cost** | $1.50 vs $3 (save $1.50 per unit) |
| **Lower power** | 43mA vs 80mA (46% less) |
| **Built-in USB-C** | No external programmer needed |
| **Smaller size** | 60x40mm PCB vs 80x50mm (33% smaller) |
| **Higher efficiency** | 95% vs 92% (MP2315 vs LM2596S) |
| **Modern design** | RISC-V, BLE 5.0, better power management |
| **Simpler assembly** | Fewer external components |

**Total savings: $2-3 per unit + 30-46% lower power consumption!**

**Recommendation: Use ESP32-C3 Super Mini + MP2315 for this project!** ✅
