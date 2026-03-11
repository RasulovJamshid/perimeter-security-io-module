# Connection Diagrams — Bolid Orion Integration

## 1. System Overview — ESP32 Gateway / Bridge

The ESP32 **passively listens** to ALL traffic on the Orion RS-485 bus.
It decodes every packet between С2000М and all devices, then forwards
device statuses, events, and alarms to external systems via:
- **Serial cable (UART)** — direct connection to another controller/PC
- **WiFi + MQTT** — Home Assistant, Node-RED, etc.
- **HTTP REST API** — direct JSON queries over WiFi

```
                        Orion RS-485 Bus (9600 8N1)
    ═══════════════════════════════════════════════════════════════

     A ──┬────────┬──────────┬──────────┬──────────┬──────────── A
         │        │          │          │          │
     B ──┼────────┼──────────┼──────────┼──────────┼──────────── B
         │        │          │          │          │
    ┌────┴───┐┌───┴───┐┌────┴───┐┌────┴───┐┌────┴────────────────────┐
    │ С2000М ││Sig-20P││С2000-4 ││С2000-2 ││   ESP32 GATEWAY         │
    │ master ││addr=1 ││addr=2  ││addr=3  ││   (passive listener)    │
    └────────┘└───────┘└────────┘└────────┘│                          │
                                           │  UART2 ◄── MAX485 (RX)  │
         ┌─────────┐                       │                          │
         │ Bolid   │                       │  UART1 ════► Serial cable│──► External
         │ Desktop │                       │              (to other   │   System
         │   App   │                       │               system)    │
         └─────────┘                       │                          │
                                           │  WiFi ═════► MQTT       │──► Home
                                           │              HTTP API   │   Assistant
                                           └──────────────────────────┘
```

**Key: The ESP32 NEVER transmits on the Orion bus. It only reads.
This means zero interference with the existing system.**

---

## 2. ESP32 Gateway — Hardware Wiring

### 2.1 Component List

| # | Component | Model | Qty | Notes |
|---|-----------|-------|:---:|-------|
| 1 | Microcontroller | ESP32 DevKit V1 | 1 | 3 UARTs: USB debug, RS-485, ext serial |
| 2 | RS-485 transceiver | MAX485 / SP3485 module | 1 | 3.3V compatible |
| 3 | Power supply | 5V 1A | 1 | USB or external |
| 4 | Serial cable | UART TTL or USB-UART | 1 | For connection to external system |
| 5 | Termination resistor | 120Ω | 1 | Only if at end of RS-485 line |

### 2.2 ESP32 ↔ MAX485 Wiring

```
                 ESP32 DevKit V1                    MAX485 Module
              ┌──────────────────┐               ┌──────────────┐
              │                  │               │              │
              │  GPIO16 (TX2) ───┼──────────────►│ DI           │
              │                  │               │              │
              │  GPIO17 (RX2) ◄──┼───────────────│ RO           │
              │                  │               │              │
              │  GPIO4  ─────────┼──────┬───────►│ DE           │
              │                  │      └───────►│ RE           │
              │                  │               │              │
              │  3V3 ────────────┼──────────────►│ VCC          │
              │                  │               │              │
              │  GND ────────────┼──────────────►│ GND          │
              │                  │               │        A ────┼──► RS-485 A
              │                  │               │        B ────┼──► RS-485 B
              └──────────────────┘               └──────────────┘
```

### 2.3 External System Serial Cable (UART1)

The ESP32 uses a **second UART** to output data to your external system.
This can be a direct TTL serial connection or via a USB-UART adapter.

```
    ESP32                              External System
    ┌────────────────┐                ┌────────────────────────┐
    │                │                │                        │
    │  GPIO25 (TX1) ─┼───────────────►│ RX                     │
    │                │                │                        │
    │  GPIO26 (RX1) ◄┼────────────────│ TX                     │
    │                │                │                        │
    │  GND ──────────┼────────────────│ GND (MUST share GND!)  │
    │                │                │                        │
    └────────────────┘                └────────────────────────┘

    Protocol: 115200 baud, 8N1, plain text lines
    Output:   STATUS,<dev>,<zone>,<code>,<string>
              DEVICE,<dev>,<ONLINE|OFFLINE>,<type>,<type_string>
              EVENT,<dev>,<zone>,<code>,<string>
    Input:    GET_STATUS / GET_DEVICE,<addr> / GET_EVENTS / PING
```

**Option A: Direct TTL connection** (3.3V logic)
- Wire GPIO25→RX, GPIO26←TX, shared GND
- For STM32, Arduino, Raspberry Pi, or any 3.3V MCU

**Option B: Via USB-UART adapter** (to PC)
- Use a CP2102/CH340 USB-UART module
- GPIO25→RX, GPIO26←TX, shared GND
- Appears as COM port on PC

**Option C: Via RS-232 level shifter** (for industrial equipment)
- Add MAX3232 between ESP32 UART1 and DB9 connector
- For PLC, SCADA, or legacy industrial systems

### 2.4 Complete ESP32 Gateway Schematic

```
                                    Orion RS-485 Bus
                                    ════════════════
                                      A         B
                                      │         │
                                  ┌───┴─────────┴───┐
                  120Ω            │   MAX485 Module  │
               (optional,         │                  │
                end of line)      │  DI  RO  DE  RE  │
                                  └──┬───┬───┬───┬──┘
                                     │   │   │   │
              ┌──────────────────────┘   │   └┬──┘
              │  (not used,              │    │
              │   stays LOW)             │    │
    ┌─────────┼──────────────────────────┼────┼───────────────┐
    │  ESP32  │                          │    │               │
    │         ▼                          ▼    ▼               │
    │    GPIO16 (TX2)              GPIO17    GPIO4            │
    │    (not used—               (RX2)   (DE/RE=LOW         │
    │     passive mode)            BUS IN   always receive)  │
    │                                                         │
    │    GPIO25 (TX1) ════════════════► External System RX    │
    │    GPIO26 (RX1) ◄════════════════ External System TX    │
    │                                                         │
    │    3V3 ──────────────────────► MAX485 VCC               │
    │    GND ──────────────────────► MAX485 GND               │
    │    GND ══════════════════════► External System GND      │
    │                                                         │
    │    USB ──── 5V Power / Programming / Debug Monitor      │
    │                                                         │
    │    WiFi ═══► MQTT Broker / HTTP REST API                │
    │                                                         │
    └─────────────────────────────────────────────────────────┘

    NOTE: GPIO16 (TX2) connects to MAX485 DI but is never used
          in passive mode. DE/RE pin stays LOW = receive only.
```

---

## 3. PC Passive Sniffer — Hardware Wiring

### 3.1 Component List

| # | Component | Model | Qty | Notes |
|---|-----------|-------|:---:|-------|
| 1 | USB-to-RS485 adapter | FTDI / CH340-based | 1 | With A/B screw terminals |

### 3.2 Sniffer Wiring (Read-Only Tap)

```
    Existing Orion RS-485 Bus (DO NOT MODIFY):

         С2000М                                    Bolid Devices
           │                                           │
     A ────┼───────────────────────────────────────────┼──── A
           │                                           │
     B ────┼───────────────────────────────────────────┼──── B
           │                                           │
           │           ┌─────────────────┐             │
           │           │ USB-to-RS485    │             │
           │      A ───┤ Adapter         │             │
           │      B ───┤ (RX only)       │             │
           │           │                 │             │
           │           └────────┬────────┘             │
           │                    │ USB                   │
           │              ┌─────┴─────┐                │
           │              │    PC     │                │
           │              │           │                │
           │              │  orion_   │                │
           │              │  example  │                │
           │              │  COM3 0   │                │
           │              │  sniff    │                │
           │              └───────────┘                │
           │                                           │
     120Ω termination                         120Ω termination
     at С2000М end                            at last device
```

**IMPORTANT**: The sniffer ONLY reads. It never transmits on the bus.
Most USB-RS485 adapters will not transmit unless explicitly told to,
so the sniffer connection is inherently safe.

---

## 4. Both Systems Combined

```
    Orion RS-485 Bus
    ════════════════════════════════════════════════════════════

     A ──┬────────┬──────────┬──────────┬──────────┬──────── A
         │        │          │          │          │
     B ──┼────────┼──────────┼──────────┼──────────┼──────── B
         │        │          │          │          │
    ┌────┴───┐┌───┴───┐┌────┴───┐┌────┴───┐┌────┴────────────┐
    │ С2000М ││Sig-20P││С2000-4 ││С2000-2 ││  ESP32 + MAX485 │
    │ master ││addr=1 ││addr=2  ││addr=3  ││  addr=10        │
    │        ││       ││        ││        ││                  │
    └────────┘└───────┘└────────┘└────────┘│  GPIO32 ◄─ PIR  │
                                           │  GPIO33 ◄─ Door │
         ┌────────────┐                    │  GPIO34 ◄─ Beam │
         │ USB-RS485  │ (sniffer tap)      │  GPIO35 ◄─ Btn  │
         │  adapter   ├─── A               │                  │
         │            ├─── B               │  WiFi ═══► MQTT  │
         └──────┬─────┘                    └──────────────────┘
                │ USB
         ┌──────┴─────┐
         │     PC     │
         │  (sniffer) │        ┌───────────────────┐
         │  or        │        │   MQTT Broker      │
         │  (master   │◄══════►│   192.168.1.100    │
         │  on sepa-  │ WiFi   │                    │
         │  rate bus)  │        │  Home Assistant    │
         └────────────┘        │  Node-RED          │
                               │  Telegram Bot      │
                               └───────────────────┘
```

---

## 5. RS-485 Bus Rules

1. **Termination**: 120Ω resistor between A and B at BOTH ends of the bus (not in the middle)
2. **Max cable length**: 1200m at 9600 baud
3. **Max devices**: 32 unit loads (standard RS-485); 127 addressable in Orion protocol
4. **Cable type**: Twisted pair, shielded (Belden 9841 or equivalent)
5. **Topology**: Daisy-chain (bus), NOT star
6. **Bias resistors**: Some adapters include 560Ω bias resistors. If the bus is noisy at idle, add 560Ω pull-up on A and 560Ω pull-down on B

```
    CORRECT (daisy-chain):
    ═══╤═══╤═══╤═══╤═══
       │   │   │   │
      Dev1 Dev2 Dev3 Dev4

    WRONG (star):
            ┌── Dev1
    ════════┼── Dev2    ← will cause signal reflections!
            ├── Dev3
            └── Dev4
```

---

## 6. Power Considerations

### ESP32 Power Options

| Option | Voltage | Notes |
|--------|---------|-------|
| USB from PC | 5V | Easiest for development |
| External 5V supply | 5V to VIN pin | For permanent installation |
| Orion bus power (12V) | Via LM7805 → VIN | Tap from bus power, add regulator |

```
    Orion 12V power rail (if available):

    +12V ──── LM7805 ──── 5V ──── ESP32 VIN
                │
    GND  ──────┴──────── GND ──── ESP32 GND

    Add 100nF ceramic + 100μF electrolytic caps on input/output of LM7805.
```

### Sensor Power

Most perimeter sensors (PIR, beam detectors) need 12V power.
If sensors are powered from the Orion bus, just wire their relay outputs to ESP32 GPIOs.

```
    12V Power ──────► Sensor VCC
    GND ─────┬──────► Sensor GND
             │
             └──────► ESP32 GND (common ground!)

    Sensor relay output:
        COM ───► ESP32 GPIOxx
        NO  ───► GND
```
