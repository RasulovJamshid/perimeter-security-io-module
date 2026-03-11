# ESP32-C3 Orion Gateway — Bill of Materials (USB-Powered)

## Optimized Design — Single USB Cable

This BOM covers all components for the USB-powered ESP32-C3 gateway with:
- Single USB-C cable (power + programming + debug serial)
- AMS1117-3.3 LDO for peripheral power (from USB 5V)
- ADM2483 isolated RS-485 (2.5kV galvanic isolation)
- Transistor-driven LEDs
- Debug button + MONITOR/MASTER mode switch

**Refer to:** `docs/esp32c3_schematic_final.md` for circuit diagram and pinout.

---

## Core Components

| Ref | Item | Qty | Part Number / Specs | Price (USD) | Notes |
|-----|------|-----|---------------------|-------------|-------|
| **Microcontroller** |
| U2 | ESP32-C3 Super Mini | 1 | ESP32-C3 Super Mini board | $1.50 | Built-in USB-C, 4MB flash, RISC-V |
| **RS-485 Transceiver (Isolated)** |
| U3 | ADM2483 Isolated Module | 1 | ADM2483 breakout board | $5.00 | 2.5kV isolation, 3.3V logic, built-in ESD |
| **Power (LDO for peripherals)** |
| U1 | AMS1117-3.3 | 1 | AMS1117-3.3 (SOT-223) | $0.20 | 5V→3.3V LDO, 1A max |
| C12 | Ceramic 10µF 16V | 1 | Samsung CL21A106KAYNNNE (0805) | $0.10 | AMS1117 input cap |
| C13 | Ceramic 22µF 16V | 1 | Samsung CL21A226MAYLNNC (0805) | $0.10 | AMS1117 output cap |
| **Resistors (0805 SMD or 1/4W THT)** |
| R3 | 10kΩ (EN pull-up) | 1 | Yageo RC0805FR-0710KL | $0.05 | Reset button pull-up |
| R4 | 10kΩ (GPIO5 pull-up) | 1 | Yageo RC0805FR-0710KL | $0.05 | Debug button pull-up |
| R11 | 10kΩ (GPIO8 pull-up) | 1 | Yageo RC0805FR-0710KL | $0.05 | Mode switch pull-up |
| R6 | 220Ω (Q1 base) | 1 | Yageo RC0805FR-07220RL | $0.05 | Status LED transistor base |
| R8 | 220Ω (Q2 base) | 1 | Yageo RC0805FR-07220RL | $0.05 | Debug LED transistor base |
| R7 | 1kΩ (LED1 series) | 1 | Yageo RC0805FR-071KL | $0.05 | Status LED current limit |
| R9 | 1kΩ (LED2 series) | 1 | Yageo RC0805FR-071KL | $0.05 | Debug LED current limit |
| R10 | 1kΩ (LED3 series) | 1 | Yageo RC0805FR-071KL | $0.05 | Power LED current limit |
| **Capacitors** |
| C10 | Ceramic 100nF 50V | 1 | Murata GRM188R71H104KA93D (0603) | $0.05 | Debug button debounce |
| **Transistors** |
| Q1, Q2 | NPN Transistor | 2 | 2N3904 (TO-92) | $0.10 | LED drivers |
| **LEDs** |
| LED1 | Green 3mm | 1 | Kingbright L-53GD | $0.10 | Status indicator |
| LED2 | Red 3mm | 1 | Kingbright L-53ID | $0.10 | Debug/mode indicator |
| LED3 | Yellow 3mm | 1 | Kingbright L-53YD | $0.10 | Power indicator (always on) |
| **Switches** |
| SW1 | Tactile Switch (reset) | 1 | Omron B3F-1000 (6x6mm) | $0.30 | Reset button (optional) |
| SW2 | Tactile Switch (debug) | 1 | Omron B3F-1000 (6x6mm) | $0.30 | Debug mode button |
| SW3 | SPDT Toggle Switch | 1 | 5mm SPDT toggle | $0.30 | MONITOR/MASTER mode switch |
| **Connectors** |
| J1 | Pin Header 3-pin 2.54mm | 1 | Standard pin header | $0.10 | External serial (GND, TX, RX) — optional |
| J2 | Screw Terminal 2-pin 5mm | 1 | Phoenix 1757019 | $0.30 | RS-485 bus (A, B) |
| **PCB** |
| — | Custom PCB | 1 | 50x35mm, 2-layer | $1.00 | JLCPCB ($5 for 5 boards) |

---

## Assembly Options

### Option A: Breadboard Prototype (Easiest)
**Total Cost: ~$9-11**

- ESP32-C3 Super Mini ($1.50)
- ADM2483 isolated RS-485 module ($5.00)
- AMS1117-3.3 breakout or bare IC ($0.20)
- Breadboard + jumper wires ($2)
- LEDs, resistors, buttons, toggle switch ($1.50)

**Pros**: No soldering, quick assembly, easy debugging
**Cons**: Larger size, less reliable connections

### Option B: Custom PCB (Professional)
**Total Cost: ~$8-9 per unit**

- All components on 50x35mm custom PCB
- ADM2483 module mounted via headers
- Screw terminal for RS-485

**Pros**: Compact, reliable, professional
**Cons**: Requires PCB design, soldering skills

---

## Cost Summary

| Build Type | Component Cost | PCB Cost | Total |
|-----------|---------------|----------|-------|
| Breadboard prototype | ~$9 | $0 | **~$9** |
| Custom PCB (1 unit) | ~$9 | $2 | **~$11** |
| Custom PCB (5 units) | ~$9 × 5 | $5 total | **~$10/unit** |
| Production (100 units) | ~$6/unit | $0.50/unit | **~$6.50/unit** |

---

## Components Removed (vs. Previous 12V Design)

The USB-powered design eliminates these components entirely:

| Removed | Reason |
|---------|--------|
| MP2315 buck converter + L1, D2, R1, R2, C4, C5 | No 12V input — USB 5V only |
| F1 (PTC fuse) | USB port has built-in overcurrent protection |
| D1 (1N5819 reverse polarity diode) | USB connector prevents reverse polarity |
| D3 (SMBJ13A TVS) | No 12V transients from Orion PSU |
| C1 (100µF input bulk cap) | Not needed for USB 5V |
| D4, D5 (TVS on RS-485 A/B) | ADM2483 has built-in ±15kV ESD protection |
| C11 (100nF A-B filter cap) | ADM2483 module has internal filtering |
| R5 (120Ω termination) | ADM2483 module handles termination |
| J3 (12V power input terminal) | No external PSU needed |
| C6-C9 (ESP32 decoupling caps) | ESP32-C3 Super Mini has onboard decoupling |

**Result:** 24 components total (down from ~40+)

---

## Sourcing Components

### Recommended Suppliers

| Supplier | Best For | Notes |
|----------|----------|-------|
| **AliExpress** | ESP32-C3 Super Mini, ADM2483 module | Cheapest, 2-4 week shipping |
| **LCSC** | AMS1117, resistors, caps (SMD) | Low-cost, fast from China |
| **Amazon** | Quick delivery, component kits | Higher prices, fast shipping |

### Key Search Terms

| Component | Search Term |
|-----------|-------------|
| ESP32-C3 Super Mini | "ESP32-C3 Super Mini USB-C" |
| ADM2483 module | "ADM2483 RS485 isolated module 3.3V" |
| AMS1117-3.3 | "AMS1117-3.3 SOT-223" or "AMS1117 module" |
| Mode switch | "SPDT mini toggle switch 5mm" |

---

## Testing Equipment Needed

| Equipment | Purpose | Cost |
|-----------|---------|------|
| Multimeter | Voltage, continuity testing | $15-50 |
| USB-C cable (data capable) | Power + programming + serial | $3-5 |
| Oscilloscope (optional) | RS-485 signal analysis | $50-500 |

---

## Safety Notes

### Electrical Safety
- ✅ 2.5kV galvanic isolation (ADM2483) between ESP32 and Orion bus
- ✅ USB overcurrent protection (built into PC USB port)
- ✅ No high-voltage components on board
- ✅ Ground loop protection via isolation barrier

### Important Warnings
- ⚠️ **Do NOT use MAX485 (non-isolated)** — ESP32 is USB-powered, ground loop will damage equipment
- ⚠️ **Do NOT connect ADM2483 GND_ISO to ESP32 GND** — defeats isolation
- ⚠️ **Use data-capable USB-C cable** — charge-only cables won't work for programming

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-10 | Initial BOM with 12V power, MAX485 |
| 2.0 | 2026-03-11 | Redesigned for USB power, ADM2483 isolated RS-485, AMS1117 LDO |

---

## Reference

- ESP32-C3 datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf
- ADM2483 datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ADM2483.pdf
- AMS1117 datasheet: http://www.advanced-monolithic.com/pdf/ds1117.pdf
