# PC RS-485 Connection — Isolated Hardware Schema

## For Use with Orion Proxy Service

This document covers the hardware needed to connect a PC running the Orion Proxy
to the Bolid Orion RS-485 bus safely.

---

## Option 1: Buy an Isolated USB-RS485 Adapter (Recommended)

**No custom hardware needed.** Buy a pre-made isolated adapter:

**Search terms:**
- "USB RS485 isolated adapter 2500V"
- "FTDI USB RS485 isolated converter"
- "industrial isolated USB to RS485"

**Requirements checklist:**
- ✅ Galvanic isolation ≥1kV (2.5kV preferred)
- ✅ Screw terminals or bare wires for A/B
- ✅ Auto direction control (no DE/RE pin needed)
- ✅ FTDI or CH340 chipset (best driver support)
- ✅ 3.3V or 5V bus compatible
- ✅ Windows + Linux driver support

**Price:** $10-20

**Connection:**
```
PC USB port ──► Isolated USB-RS485 adapter ──► A, B wires ──► Orion bus
                     (isolation inside)
```

**Wiring (2 wires only):**
```
Adapter terminal A ──────► Orion device A (e.g. Signal-20M terminal A)
Adapter terminal B ──────► Orion device B (e.g. Signal-20M terminal B)
GND — do NOT connect (isolation handles it)
```

**That's it. No PCB, no soldering, no external components.**

---

## Option 2: Build Isolation Board (If Using Non-Isolated Adapter)

If you already have a **non-isolated** USB-RS485 adapter (like CH340-based module),
you can add isolation between it and the Orion bus.

### Circuit Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  PC-SIDE ISOLATED RS-485 INTERFACE                         │
│                                                                             │
│  PC SIDE (USB ground domain)        ║    BUS SIDE (Orion ground domain)    │
│                                      ║                                      │
│  USB-RS485 Adapter                   ║    To Orion Bus                     │
│  ┌──────────────┐                    ║                                      │
│  │ CH340/FTDI   │                    ║                                      │
│  │              │                    ║                                      │
│  │  A ──────────┼──► ADUM1201 Ch1 ──╫──► MAX485 DI ──┐                    │
│  │  B ◄─────────┼──── ADUM1201 Ch2 ◄╫─── MAX485 RO   │                    │
│  │  GND ────────┼──┐                 ║                 ├──► A ──► Orion A  │
│  └──────────────┘  │                 ║                 │                    │
│                    │                 ║    MAX485 DE ───┤                    │
│                    │                 ║    MAX485 RE ───┘──► B ──► Orion B  │
│                    │                 ║                                      │
│                    │                 ║    GND_ISO ─────────► Float         │
│                    │                 ║                                      │
│                    GND (PC)          ║    GND_ISO (Orion)                  │
│                                      ║                                      │
└─────────────────────────────────────────────────────────────────────────────┘
```

**WAIT — this doesn't work for non-isolated USB-RS485 adapters** because:
- The adapter already converts USB → RS-485 internally
- You can't split the A/B signals through isolators easily
- The adapter's A/B are already RS-485 differential signals

### Correct Approach: Isolate at UART Level

If your USB-RS485 adapter is non-isolated, the correct solution is to use a
**USB isolator** between PC and adapter:

```
PC USB ──► USB Isolator (ADUM4160) ──► Non-isolated USB-RS485 adapter ──► Orion bus
```

**OR** build the full chain from scratch:

```
PC USB ──► USB-UART chip (CH340) ──► ADUM1201 ──► MAX485 ──► Orion bus
               UART TX/RX              isolation     RS-485
```

### Full Custom Build Schematic

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│   PC SIDE (USB ground)               ║   BUS SIDE (Orion ground)           │
│                                       ║                                     │
│                                       ║                                     │
│   ┌──────────┐     ┌──────────┐      ║    ┌──────────┐    ┌─────────────┐ │
│   │  CH340G  │     │ ADUM1201 │      ║    │ ADUM1200 │    │   MAX485    │ │
│   │ USB-UART │     │ (2-ch)   │      ║    │ (2-ch)   │    │  RS-485     │ │
│   │          │     │          │      ║    │          │    │             │ │
│   │ USB ◄──► │     │ VDD1     │      ║    │     VDD2 │    │ VCC ─ 3.3V │ │
│   │ D+  D-  │     │  │       │      ║    │      │   │    │   ISO      │ │
│   │          │     │  3.3V    │      ║    │    3.3V  │    │             │ │
│   │ TXD ─────┼───► │ VIA ──► VOA ───╫──► │ VIA    ──┼──► │ DI          │ │
│   │          │     │          │      ║    │          │    │             │ │
│   │ RXD ◄────┼──── │ VOB ◄── VIB ◄──╫─── │    ◄─ VOA┼─── │ RO          │ │
│   │          │     │          │      ║    │          │    │         A ──┼─► Orion A
│   │ RTS ─────┼───► │         (spare)─╫──► │ VIB  ───┼──► │ DE ─┐       │ │
│   │          │     │          │      ║    │          │    │     ├──┐    │ │
│   │ GND ─────┼──── │ GND1    │      ║    │    GND2 ─┼─── │ RE ─┘  │    │ │
│   └──────────┘     └──────────┘      ║    └──────────┘    │     B ──┼─► Orion B
│        │                │            ║          │         │  GND    │ │
│        └────────────────┘            ║          └─────────┼────┘    │ │
│              GND_PC                  ║            GND_ISO  └─────────┘ │
│                                       ║                                 │
│   ┌──────────┐                       ║    ┌──────────┐                 │
│   │ 3.3V LDO │ ◄── USB 5V           ║    │ 3.3V LDO │ ◄── B0505S out │
│   │ (PC side)│                       ║    │ (ISO side)│                 │
│   └──────────┘                       ║    └──────────┘                 │
│        │                             ║          │                       │
│   Powers: CH340, ADUM1201 VDD1       ║   Powers: ADUM1200 VDD2, MAX485 │
│                                       ║                                 │
│              ┌────────────┐          ║                                 │
│   USB 5V ──► │  B0505S    │──────────╫──► 5V_ISO                      │
│              │  DC-DC     │          ║                                 │
│   GND_PC ──► │  Isolated  │          ║                                 │
│              └────────────┘          ║                                 │
│                                       ║                                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

### BOM for Custom Isolation Board

| Ref | Component | Part Number | Qty | Price | Purpose |
|-----|-----------|-------------|-----|-------|---------|
| U1 | USB-UART converter | CH340G (SOP-16) | 1 | $0.50 | USB to UART |
| U2 | Digital isolator (fwd) | ADUM1201ARZ (SOIC-8) | 1 | $2.00 | TX + RTS isolation |
| U3 | Digital isolator (rev) | ADUM1200ARZ (SOIC-8) | 1 | $2.00 | RX isolation |
| U4 | RS-485 transceiver | MAX485ESA+ (SOIC-8) | 1 | $1.00 | UART → differential |
| U5 | Isolated DC-DC | B0505S-1W (SIP-4) | 1 | $2.00 | 5V → 5V isolated |
| U6 | LDO (PC side) | AMS1117-3.3 (SOT-223) | 1 | $0.20 | 5V → 3.3V PC side |
| U7 | LDO (ISO side) | AMS1117-3.3 (SOT-223) | 1 | $0.20 | 5V → 3.3V ISO side |
| C1-C4 | Decoupling caps | 100nF ceramic (0805) | 4 | $0.20 | IC decoupling |
| C5-C6 | Bulk caps | 10µF ceramic (0805) | 2 | $0.20 | LDO input caps |
| C7-C8 | Output caps | 22µF ceramic (0805) | 2 | $0.20 | LDO output caps |
| C9 | USB cap | 100nF ceramic (0805) | 1 | $0.05 | CH340 USB decoupling |
| C10 | Crystal load | 22pF ceramic (0603) | 2 | $0.10 | CH340 crystal |
| Y1 | Crystal | 12MHz (HC-49S) | 1 | $0.30 | CH340 clock |
| R1 | Termination | 120Ω (0805) | 1 | $0.05 | Optional, bus end only |
| J1 | USB connector | USB-B or Micro-B | 1 | $0.30 | PC connection |
| J2 | Screw terminal | 2-pin 5mm | 1 | $0.30 | RS-485 A/B out |
| | **Total** | | | **~$9.60** | |

### Signal Flow

```
PC USB (D+/D-) 
    ↓
CH340G → TXD (3.3V UART) → ADUM1201 Ch1 → (isolation) → ADUM1200 output → MAX485 DI
                                                                              ↓
CH340G ← RXD (3.3V UART) ← ADUM1201 Ch2 ← (isolation) ← ADUM1200 input ← MAX485 RO
                                                                              ↓
CH340G → RTS → ADUM1200 spare channel → (isolation) → MAX485 DE+RE            ↓
                                                                           A, B → Orion bus
```

### DE/RE Control

**Auto-direction using CH340 RTS pin:**
- CH340 sets RTS HIGH before transmitting
- RTS goes through isolator to MAX485 DE/RE
- MAX485 switches to transmit mode
- After TX complete, CH340 drops RTS LOW
- MAX485 returns to receive mode

**OR for passive MONITOR mode only:**
- Tie DE/RE LOW permanently (receive only)
- No need for third isolation channel
- Can use single ADUM1201 (2 channels: TX + RX)

---

## Option 3: USB Isolator + Standard USB-RS485 (Simplest DIY)

**Buy two separate modules:**

1. **USB Isolator module** (~$8-15)
   - Search: "ADUM4160 USB isolator module"
   - Provides 2.5kV USB isolation
   - Has USB-A input and USB-A/B output

2. **Standard USB-RS485 adapter** (~$3-5)
   - Search: "USB RS485 converter CH340"
   - Any cheap non-isolated adapter

**Connection:**
```
PC USB ──► USB Isolator Module ──► USB-RS485 Adapter ──► Orion bus A, B
              (ADUM4160)              (CH340/FTDI)
```

**Total cost:** ~$11-20
**Soldering:** None
**Advantages:** Modular, replaceable, no PCB design

---

## Recommendation Summary

| Option | Cost | Difficulty | Reliability | Recommended |
|--------|------|-----------|-------------|-------------|
| **1. Isolated USB-RS485** | $10-20 | None | ★★★★★ | ✅ Best choice |
| **2. Custom board** | ~$10 | Hard (SMD soldering) | ★★★★ | For custom PCB |
| **3. USB isolator + adapter** | $11-20 | None | ★★★★ | ✅ Good fallback |

**Go with Option 1** if you can find an isolated adapter.
**Go with Option 3** if isolated adapters are unavailable.
**Go with Option 2** only if you're making a production PCB.

---

## Connection to Orion Devices

### C2000M Panel (Master Controller)

```
C2000M RS-485 Terminals
  TB3 or marked "RS-485":
    A ──────► Adapter A
    B ──────► Adapter B
    GND ────  DO NOT CONNECT (isolation!)
```

### Signal-20M (Zone Expander)

```
Signal-20M Terminals:
    A ──────► Adapter A
    B ──────► Adapter B
    +12V ───  DO NOT CONNECT
    GND ────  DO NOT CONNECT
```

### Any Orion RS-485 Device

You can tap at **any device** on the bus — parallel connection:

```
Existing bus:
  C2000M ──── Device 1 ──── Device 2 ──── Signal-20M
                                │
                         Your adapter A, B
                         (parallel tap here)
```

### Termination

- **If adapter is at bus end:** Enable 120Ω termination (jumper on adapter, or add resistor)
- **If adapter is mid-bus:** No termination needed

---

## Software Configuration

After connecting hardware, start the Orion Proxy:

```bash
# Windows — find your COM port in Device Manager
orion_proxy.exe -p COM5 -b 9600 -t 9100

# Linux — find your port with: ls /dev/ttyUSB*
./orion_proxy -p /dev/ttyUSB0 -b 9600 -t 9100
```

See `pc/orion_proxy/README.md` for full proxy documentation.
