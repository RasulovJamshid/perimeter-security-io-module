# Orion Proxy Installation Guide for Ubuntu

This guide covers installing and running the Orion RS-485 Proxy on Ubuntu Linux.

---

## Prerequisites

- Ubuntu 18.04 or newer (tested on 20.04, 22.04)
- USB-to-RS485 adapter (or virtual COM ports for testing)
- Root/sudo access for serial port permissions

---

## Installation Steps

### 1. Build the Application

If you haven't built it yet, follow the [Linux Build Guide](LINUX_BUILD_GUIDE.md):

```bash
# Install build dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake

# Navigate to project directory
cd /path/to/perimiter_security/pc/orion_proxy

# Build
mkdir build && cd build
cmake ..
make

# Verify binary was created
ls -lh orion_proxy
```

### 2. Install to System (Optional)

You can run the binary directly from the build directory, or install it system-wide:

```bash
# Copy binary to /usr/local/bin
sudo cp orion_proxy /usr/local/bin/

# Make it executable
sudo chmod +x /usr/local/bin/orion_proxy

# Verify installation
which orion_proxy
orion_proxy --help
```

### 3. Configure Serial Port Permissions

By default, serial ports require root access. Add your user to the `dialout` group:

```bash
# Add current user to dialout group
sudo usermod -a -G dialout $USER

# Log out and log back in for changes to take effect
# Or use: newgrp dialout

# Verify group membership
groups | grep dialout
```

### 4. Identify Your Serial Port

Find your USB-to-RS485 adapter:

```bash
# List all serial devices
ls -l /dev/ttyUSB* /dev/ttyACM*

# Or use dmesg to see recent USB connections
dmesg | grep tty

# Common device names:
# /dev/ttyUSB0 - USB-to-serial adapters
# /dev/ttyACM0 - Arduino-based adapters
# /dev/ttyS0   - Built-in serial ports
```

### 5. Create Configuration File

Create `orion_proxy.ini` in your working directory:

```ini
[serial]
port = /dev/ttyUSB0
baud_rate = 9600

[server]
tcp_port = 9100
http_port = 8080

[protocol]
mode = monitor
global_key = 0
auto_scan = false

[debug]
debug_level = 1
verbose = true
log_file =
```

Adjust `port` to match your actual device (from step 4).

---

## Running the Application

### Basic Usage

```bash
# Run with default settings (looks for orion_proxy.ini)
./orion_proxy

# Specify serial port manually
./orion_proxy -p /dev/ttyUSB0

# Run with verbose output
./orion_proxy -p /dev/ttyUSB0 -v -d 2

# Run in master mode with auto-scan
./orion_proxy -p /dev/ttyUSB0 -m --auto-scan
```

### Run as Background Service

Create a systemd service file:

```bash
sudo nano /etc/systemd/system/orion-proxy.service
```

Add the following content:

```ini
[Unit]
Description=Orion RS-485 Proxy Service
After=network.target

[Service]
Type=simple
User=YOUR_USERNAME
WorkingDirectory=/home/YOUR_USERNAME/orion_proxy
ExecStart=/usr/local/bin/orion_proxy -c /home/YOUR_USERNAME/orion_proxy/orion_proxy.ini
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Replace `YOUR_USERNAME` with your actual username.

Enable and start the service:

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service to start on boot
sudo systemctl enable orion-proxy

# Start the service
sudo systemctl start orion-proxy

# Check status
sudo systemctl status orion-proxy

# View logs
sudo journalctl -u orion-proxy -f
```

---

## Accessing the Dashboard

Once running, open the web dashboard:

```bash
# On the same machine
firefox http://localhost:8080

# From another machine on the network
firefox http://YOUR_UBUNTU_IP:8080
```

---

## Testing Without Hardware

For testing without real RS-485 devices, use virtual serial ports:

```bash
# Install socat (virtual serial port tool)
sudo apt-get install socat

# Create virtual COM port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0

# Output will show something like:
# 2024/03/12 15:30:00 socat[12345] N PTY is /dev/pts/2
# 2024/03/12 15:30:00 socat[12345] N PTY is /dev/pts/3

# Use one port for orion_proxy, the other for the simulator
./orion_proxy -p /dev/pts/2 -v -d 2

# In another terminal, run the Python simulator
cd tools
python3 test_simulator.py /dev/pts/3
```

See [Testing Without Hardware Guide](TESTING_WITHOUT_HARDWARE.md) for more details.

---

## TCP Client Connection

Connect to the proxy's TCP interface:

```bash
# Using telnet
telnet localhost 9100

# Using netcat
nc localhost 9100

# Type HELP to see available commands
HELP
```

Available TCP commands:
- `HELP` - Show command list
- `STATUS` - Show proxy status
- `DEVICES` - List discovered devices
- `STATS` - Show statistics
- `CONFIG` - Show current configuration
- `DEVICE_TYPES` - List known device types

---

## Troubleshooting

### Permission Denied on Serial Port

```bash
# Check current permissions
ls -l /dev/ttyUSB0

# Should show: crw-rw---- 1 root dialout

# If not in dialout group:
sudo usermod -a -G dialout $USER
# Then log out and back in
```

### Port Already in Use

```bash
# Find what's using the port
sudo lsof /dev/ttyUSB0

# Kill the process if needed
sudo killall -9 process_name
```

### Dashboard Not Accessible

```bash
# Check if port 8080 is listening
sudo netstat -tlnp | grep 8080

# Check firewall (if enabled)
sudo ufw status
sudo ufw allow 8080/tcp
```

### Binary Not Found After Install

```bash
# Check if /usr/local/bin is in PATH
echo $PATH | grep /usr/local/bin

# If not, add to ~/.bashrc
echo 'export PATH="/usr/local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

---

## Uninstallation

```bash
# Stop service (if running)
sudo systemctl stop orion-proxy
sudo systemctl disable orion-proxy

# Remove service file
sudo rm /etc/systemd/system/orion-proxy.service
sudo systemctl daemon-reload

# Remove binary
sudo rm /usr/local/bin/orion_proxy

# Remove config (optional)
rm ~/orion_proxy/orion_proxy.ini
```

---

## Next Steps

- Read the [PC Proxy Guide](PC_PROXY_GUIDE.md) for detailed usage
- Check [Protocol Compatibility](PROTOCOL_COMPATIBILITY.md) for Bolid device support
- Review [Testing Without Hardware](TESTING_WITHOUT_HARDWARE.md) for safe testing

---

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review application logs: `sudo journalctl -u orion-proxy`
3. Run with verbose debug: `./orion_proxy -v -d 3`
