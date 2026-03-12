# Testing Orion Proxy Without Real Hardware

This guide shows you how to test the Orion Proxy application without connecting to a real RS-485 bus or visiting the installation site.

---

## Table of Contents

1. [Option 1: Virtual COM Port Pair (Recommended)](#option-1-virtual-com-port-pair-recommended)
2. [Option 2: Dashboard-Only Mode](#option-2-dashboard-only-mode)
3. [Option 3: Recorded Packet Playback](#option-3-recorded-packet-playback)
4. [Testing Checklist](#testing-checklist)

---

## Option 1: Virtual COM Port Pair (Recommended)

Create a **loopback COM port pair** and use a Python simulator to send fake Orion packets.

### What You Need

- **com0com** (Windows) — Free virtual COM port driver
- **Python 3.x** with `pyserial` library
- The packet simulator script (included)

### Step-by-Step Setup

#### 1. Install com0com

**Download:** https://sourceforge.net/projects/com0com/

1. Download `com0com-3.0.0.0-i386-and-x64-signed.zip`
2. Extract and run `setup_com0com_W7_x64_signed.exe` (or x86 version)
3. Follow the installer prompts
4. **Important:** Allow the unsigned driver if Windows asks

#### 2. Create Virtual COM Port Pair

1. Open **com0com Setup** from Start Menu
2. Click **Add Pair**
3. You'll see something like:
   ```
   CNCA0 <-> CNCB0
   ```
4. Change port names to real COM ports:
   - Right-click `CNCA0` → **Change Settings**
   - Set **Port Name** to `COM10`
   - Click **Apply**
   - Repeat for `CNCB0` → `COM11`
5. You now have: `COM10 <-> COM11` (anything sent to COM10 appears on COM11 and vice versa)

**Alternative for Linux:**

```bash
# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the device names (e.g., /dev/pts/2 and /dev/pts/3)
```

#### 3. Install Python Dependencies

```bash
pip install pyserial
```

#### 4. Run the Packet Simulator

```bash
cd pc/orion_proxy/tools
python test_simulator.py COM11
```

You'll see:
```
[SIM] Connected to COM11 at 9600 baud
[SIM] Simulating 3 Orion devices...
[SIM] Press Ctrl+C to stop

[SIM] Sent device info: addr=1 type=0x01 (C2000M)
[SIM] Sent device info: addr=5 type=0x06 (Signal-20M)
[SIM] Sent device info: addr=10 type=0x10 (S2000-4)
[SIM] Sent status: addr=1 status=0x00
[SIM] Sent status: addr=5 status=0x00
[SIM] Sent event: addr=5 code=0x81 zone=3
...
```

#### 5. Run the Proxy

In another terminal:

```bash
cd pc/orion_proxy/build
orion_proxy.exe -p COM10 -v
```

You'll see the proxy receiving packets:
```
[MAIN] Serial port COM10 opened at 9600 baud
[MAIN] Sniffer thread started (MONITOR mode)
[PACKET] dev=1 model=C2000M len=8 enc=0
[PACKET] dev=5 model=Signal-20M len=8 enc=0
[PACKET] dev=10 model=S2000-4 len=8 enc=0
```

#### 6. Open the Dashboard

Navigate to `http://localhost:8080` — you'll see:
- **3 devices online** (C2000M, Signal-20M, S2000-4)
- **Live packet count** increasing
- **Events** appearing in real-time

### What the Simulator Does

The `test_simulator.py` script:
- Simulates **3 Orion devices** (addresses 1, 5, 10)
- Sends **device info responses** every 20 seconds
- Sends **status updates** every 2 seconds
- Randomly generates **alarm events** (zone alarms, tamper, restore)
- Uses **correct Orion protocol** (CRC-8, packet structure)

### Customizing the Simulator

Edit `test_simulator.py` to:

**Add more devices:**
```python
devices = [
    {'addr': 1, 'type': 0x01, 'name': 'C2000M'},
    {'addr': 5, 'type': 0x06, 'name': 'Signal-20M'},
    {'addr': 10, 'type': 0x10, 'name': 'S2000-4'},
    {'addr': 15, 'type': 0x16, 'name': 'S2000-2'},  # Add this
]
```

**Change packet frequency:**
```python
time.sleep(2)  # Change to 0.5 for faster packets
```

**Simulate specific events:**
```python
# Force a zone 5 alarm on device 5
pkt = simulate_event_response(5, 0x81, 5)
port.write(pkt)
```

---

## Option 2: Dashboard-Only Mode

Test the **web dashboard and configuration** without any serial port.

### How to Use

Simply run the proxy without a valid COM port:

```bash
orion_proxy.exe
```

Or with an invalid port:

```bash
orion_proxy.exe -p COM99
```

You'll see:
```
[WARN] Failed to open serial port: COM99
[WARN] RS-485 bus will NOT be monitored.
[WARN] Dashboard still available at http://localhost:8080
```

### What You Can Test

- ✅ **Web dashboard** loads and renders correctly
- ✅ **Configuration tab** — change settings, save to file
- ✅ **TCP server** — connect with `telnet localhost 9100`
- ✅ **REST API** — test with curl or Postman
- ✅ **Tray mode** — `--tray` flag works
- ❌ No device data (0 devices, 0 packets)

### Use Cases

- **UI development** — test dashboard changes
- **Configuration testing** — verify INI file load/save
- **Client app development** — test TCP/HTTP API without hardware
- **Deployment testing** — verify exe runs on target Windows version

---

## Option 3: Recorded Packet Playback

Capture real packets from a site visit, then replay them for testing.

### Step 1: Capture Packets

At the real site, run the proxy with debug mode:

```bash
orion_proxy.exe -p COM5 -d 3 --log capture.log
```

This logs **all packets in hex format** to `capture.log`.

### Step 2: Extract Hex Packets

From `capture.log`, you'll see lines like:
```
[2024-03-12 10:15:23] PKT dev=5 model=Signal-20M len=8 enc=0 hex=FF050801123456789A
```

Extract the hex strings:
```
FF050801123456789A
FF0A0C02000102030405060708090A0BCD
...
```

### Step 3: Create Playback Script

```python
# playback.py
import serial
import time

packets = [
    bytes.fromhex("FF050801123456789A"),
    bytes.fromhex("FF0A0C02000102030405060708090A0BCD"),
    # ... add more
]

port = serial.Serial('COM11', 9600)  # Virtual port
for pkt in packets:
    port.write(pkt)
    print(f"Sent: {pkt.hex()}")
    time.sleep(0.5)
port.close()
```

### Step 4: Replay

```bash
# Terminal 1: Run proxy
orion_proxy.exe -p COM10

# Terminal 2: Playback packets
python playback.py
```

The proxy will process the recorded packets as if they were live.

---

## Testing Checklist

Use this checklist to verify all features work:

### Basic Functionality

- [ ] Proxy starts without errors
- [ ] Serial port opens (real or virtual)
- [ ] Dashboard loads at `http://localhost:8080`
- [ ] TCP server accepts connections on port 9100

### Device Tracking

- [ ] Devices appear in dashboard "Devices" tab
- [ ] Device models identified correctly (Signal-20M, C2000M, etc.)
- [ ] Online/offline status updates
- [ ] Packet count increments

### Events

- [ ] Events appear in "Live Events" tab
- [ ] Auto-scroll works
- [ ] Event types color-coded (packet, alarm, error)
- [ ] Event buffer limited to last 256 events

### Configuration

- [ ] Config file loads on startup
- [ ] Command-line args override config file
- [ ] Runtime changes via dashboard work
- [ ] "Save to File" persists settings
- [ ] `--save-config` generates valid INI file

### TCP API

- [ ] `telnet localhost 9100` connects
- [ ] `HELP` command lists all commands
- [ ] `GET_STATUS` returns valid data
- [ ] `GET_DEVICES` lists devices
- [ ] `SUBSCRIBE_EVENTS` pushes events
- [ ] `QUIT` disconnects cleanly

### HTTP API

- [ ] `GET /api/status` returns JSON
- [ ] `GET /api/devices` returns device array
- [ ] `GET /api/live_events` returns events
- [ ] `POST /api/command` executes commands
- [ ] `POST /api/set` changes config
- [ ] `POST /api/save_config` writes file

### Run Modes

- [ ] Console mode shows output
- [ ] `--tray` mode hides console
- [ ] Tray icon appears in notification area
- [ ] Double-click tray → opens dashboard
- [ ] Right-click tray → shows menu
- [ ] `--background` runs headless
- [ ] Logging to file works

### MASTER Mode (if testing active commands)

- [ ] `-m` flag enables MASTER mode
- [ ] `SCAN` command queries all devices
- [ ] `SCAN_DEVICE,<addr>` queries single device
- [ ] `READ_STATUS,<addr>` returns status
- [ ] `--auto-scan` scans on startup

### Error Handling

- [ ] Invalid COM port → warning, dashboard still works
- [ ] Port in use → error message
- [ ] No dashboard.html → 404 error
- [ ] Invalid config file → uses defaults
- [ ] Ctrl+C → clean shutdown

### Performance

- [ ] Dashboard responsive with 100+ packets/sec
- [ ] No memory leaks after 1 hour
- [ ] CPU usage < 5% when idle
- [ ] TCP server handles 10+ simultaneous clients

---

## Common Testing Scenarios

### Scenario 1: New Installation Test

```bash
# 1. Test without hardware
orion_proxy.exe
# → Dashboard should load, 0 devices

# 2. Test with virtual COM port + simulator
orion_proxy.exe -p COM10 -v
python test_simulator.py COM11
# → Should see 3 devices, events

# 3. Test tray mode
orion_proxy.exe -p COM10 --tray
# → Console hidden, tray icon visible, dashboard auto-opens
```

### Scenario 2: Configuration Test

```bash
# 1. Generate default config
orion_proxy.exe --save-config

# 2. Edit orion_proxy.ini
# Change tcp_port = 9200

# 3. Restart and verify
orion_proxy.exe -p COM10
# → Should listen on port 9200 instead of 9100
```

### Scenario 3: Client App Development

```bash
# 1. Run proxy in dashboard-only mode
orion_proxy.exe -p COM99

# 2. Test REST API
curl http://localhost:8080/api/status
curl http://localhost:8080/api/devices

# 3. Test TCP API
telnet localhost 9100
> HELP
> GET_STATUS
> QUIT
```

### Scenario 4: Stress Test

```python
# stress_test.py
import socket
import time

# Connect 10 TCP clients
clients = []
for i in range(10):
    s = socket.socket()
    s.connect(('localhost', 9100))
    s.recv(1024)  # Welcome
    s.sendall(b'SUBSCRIBE_EVENTS\n')
    clients.append(s)
    print(f"Client {i+1} connected")

# Let them run for 5 minutes
time.sleep(300)

# Disconnect all
for s in clients:
    s.sendall(b'QUIT\n')
    s.close()
```

Run simulator at high speed:
```python
# In test_simulator.py, change:
time.sleep(0.1)  # Send packets every 100ms
```

Verify:
- No crashes
- All clients receive events
- Dashboard still responsive

---

## Troubleshooting Virtual COM Ports

### com0com Not Working

**Problem:** "Port not found" or "Access denied"

**Solutions:**
1. Run com0com Setup as Administrator
2. Disable driver signature enforcement (Windows 10+):
   ```
   bcdedit /set testsigning on
   # Reboot
   ```
3. Use **com2tcp** as alternative (network-based virtual ports)

### Simulator Not Sending Packets

**Problem:** Proxy shows 0 packets

**Check:**
1. COM port pair is correct (`COM10 <-> COM11`)
2. Proxy is on one port, simulator on the other
3. Baud rate matches (9600)
4. No other program using the ports

**Debug:**
```bash
# In simulator, add verbose output
port.write(pkt)
print(f"Sent {len(pkt)} bytes: {pkt.hex()}")
```

### Linux Virtual Ports

Use `socat`:
```bash
# Create pair
socat -d -d pty,raw,echo=0,link=/tmp/vcom0 pty,raw,echo=0,link=/tmp/vcom1

# Run proxy
./orion_proxy -p /tmp/vcom0

# Run simulator
python test_simulator.py /tmp/vcom1
```

---

## Next Steps

After testing with virtual hardware:

1. **Deploy to real site** with confidence
2. **Capture real packets** for regression testing
3. **Build test suite** using recorded data
4. **Automate testing** with CI/CD pipeline

For real hardware setup, see `docs/pc_rs485_hardware.md`.
