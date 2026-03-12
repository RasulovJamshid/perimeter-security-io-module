# Building Orion Proxy for Linux

This guide shows you how to compile the Orion Proxy application on Linux systems.

---

## Quick Start

```bash
# Install dependencies
sudo apt-get install build-essential cmake git

# Clone and build
cd /path/to/perimiter_security/pc/orion_proxy
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run
./orion_proxy -p /dev/ttyUSB0
```

---

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Installing Dependencies](#installing-dependencies)
3. [Building from Source](#building-from-source)
4. [Installation](#installation)
5. [Running the Application](#running-the-application)
6. [Troubleshooting](#troubleshooting)
7. [Cross-Compilation](#cross-compilation)

---

## System Requirements

### Supported Distributions

| Distribution | Versions | Status |
|-------------|----------|--------|
| **Ubuntu/Debian** | 18.04+ | ✅ Tested |
| **CentOS/RHEL** | 7+ | ✅ Compatible |
| **Fedora** | 30+ | ✅ Compatible |
| **Arch Linux** | Latest | ✅ Compatible |
| **Raspberry Pi OS** | Buster+ | ✅ Tested (ARM) |

### Minimum Requirements

- **CPU:** Any x86_64 or ARM (Raspberry Pi)
- **RAM:** 64 MB
- **Disk:** 10 MB for executable + dependencies
- **Kernel:** 2.6.32+ (for serial port support)

### Build Tools

- **GCC** 4.8+ or **Clang** 3.5+
- **CMake** 3.14+
- **Make** or **Ninja**
- **Git** (optional, for cloning)

---

## Installing Dependencies

### Ubuntu/Debian

```bash
# Update package list
sudo apt-get update

# Install build tools
sudo apt-get install -y \
    build-essential \
    cmake \
    git

# Optional: Install Ninja for faster builds
sudo apt-get install -y ninja-build
```

### CentOS/RHEL 7

```bash
# Enable EPEL repository
sudo yum install -y epel-release

# Install build tools
sudo yum install -y \
    gcc \
    gcc-c++ \
    make \
    cmake3 \
    git

# Use cmake3 instead of cmake
alias cmake=cmake3
```

### CentOS/RHEL 8+

```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    cmake \
    git
```

### Fedora

```bash
sudo dnf install -y \
    gcc \
    gcc-c++ \
    make \
    cmake \
    git
```

### Arch Linux

```bash
sudo pacman -S --needed \
    base-devel \
    cmake \
    git
```

### Raspberry Pi OS

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git
```

---

## Building from Source

### Step 1: Navigate to Project Directory

```bash
cd /path/to/perimiter_security/pc/orion_proxy
```

If you don't have the source yet:

```bash
git clone <your-repo-url> perimiter_security
cd perimiter_security/pc/orion_proxy
```

### Step 2: Create Build Directory

```bash
mkdir build
cd build
```

### Step 3: Configure with CMake

**Standard build:**
```bash
cmake ..
```

**Release build (optimized):**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**Debug build (with symbols):**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

**With Ninja (faster):**
```bash
cmake -G Ninja ..
```

### Step 4: Compile

**With Make:**
```bash
make -j$(nproc)
```

**With Ninja:**
```bash
ninja
```

### Step 5: Verify Build

```bash
./orion_proxy --help
```

You should see:
```
Orion RS-485 Proxy Service v1.0
Connects to Orion RS-485 bus via USB adapter, decodes protocol,
and serves data to client applications over TCP.

Usage: ./orion_proxy [options]
...
```

---

## Installation

### Option 1: Run from Build Directory

```bash
cd build
./orion_proxy -p /dev/ttyUSB0
```

### Option 2: Install System-Wide

```bash
# From build directory
sudo make install

# Or manually
sudo cp orion_proxy /usr/local/bin/
sudo cp ../gui/dashboard.html /usr/local/share/orion_proxy/
sudo chmod +x /usr/local/bin/orion_proxy
```

### Option 3: Create Portable Package

```bash
# Create deployment folder
mkdir -p ~/orion_proxy_deploy
cp orion_proxy ~/orion_proxy_deploy/
cp ../gui/dashboard.html ~/orion_proxy_deploy/

# Create launcher script
cat > ~/orion_proxy_deploy/run.sh << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"
./orion_proxy -p /dev/ttyUSB0 "$@"
EOF

chmod +x ~/orion_proxy_deploy/run.sh

# Package for distribution
tar -czf orion_proxy_linux_x64.tar.gz -C ~/ orion_proxy_deploy
```

---

## Running the Application

### Basic Usage

```bash
# Find your USB-RS485 adapter
ls /dev/ttyUSB* /dev/ttyACM*

# Run proxy
./orion_proxy -p /dev/ttyUSB0
```

### Serial Port Permissions

If you get "Permission denied":

**Option 1: Add user to dialout group (recommended)**
```bash
sudo usermod -aG dialout $USER
# Log out and back in for changes to take effect
```

**Option 2: Run with sudo (not recommended)**
```bash
sudo ./orion_proxy -p /dev/ttyUSB0
```

**Option 3: Set udev rules**
```bash
# Create udev rule
sudo tee /etc/udev/rules.d/99-usb-serial.rules << EOF
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666"
EOF

# Reload rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Unplug and replug adapter
```

### Running as System Service

Create systemd service file:

```bash
sudo tee /etc/systemd/system/orion-proxy.service << EOF
[Unit]
Description=Orion RS-485 Proxy Service
After=network.target

[Service]
Type=simple
User=orion
Group=dialout
WorkingDirectory=/opt/orion_proxy
ExecStart=/opt/orion_proxy/orion_proxy -p /dev/ttyUSB0 --background
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Create user
sudo useradd -r -s /bin/false orion
sudo usermod -aG dialout orion

# Install files
sudo mkdir -p /opt/orion_proxy
sudo cp orion_proxy /opt/orion_proxy/
sudo cp ../gui/dashboard.html /opt/orion_proxy/
sudo chown -R orion:dialout /opt/orion_proxy

# Enable and start
sudo systemctl daemon-reload
sudo systemctl enable orion-proxy
sudo systemctl start orion-proxy

# Check status
sudo systemctl status orion-proxy
```

### Accessing Dashboard

```bash
# Local access
firefox http://localhost:8080

# Or with curl
curl http://localhost:8080/api/status
```

---

## Troubleshooting

### Build Errors

**Error: `cmake: command not found`**
```bash
# Install CMake
sudo apt-get install cmake  # Ubuntu/Debian
sudo yum install cmake3      # CentOS 7
```

**Error: `CMake 3.14 or higher is required`**
```bash
# Ubuntu 18.04: Install from Kitware repository
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | sudo apt-key add -
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
sudo apt-get update
sudo apt-get install cmake

# Or build from source
wget https://github.com/Kitware/CMake/releases/download/v3.27.0/cmake-3.27.0.tar.gz
tar -xzf cmake-3.27.0.tar.gz
cd cmake-3.27.0
./bootstrap && make -j$(nproc) && sudo make install
```

**Error: `pthread_create undefined reference`**

This shouldn't happen with CMake, but if it does:
```bash
# Manually link pthread
gcc -o orion_proxy *.o -lpthread
```

### Runtime Errors

**Error: `Failed to open serial port: /dev/ttyUSB0`**

Check permissions:
```bash
ls -l /dev/ttyUSB0
# Should show: crw-rw---- 1 root dialout

# Check if you're in dialout group
groups
# Should include: dialout

# If not, add yourself
sudo usermod -aG dialout $USER
# Then log out and back in
```

**Error: `Address already in use` (port 9100 or 8080)**

```bash
# Find what's using the port
sudo netstat -tulpn | grep :9100
sudo lsof -i :9100

# Kill the process or change port
./orion_proxy -p /dev/ttyUSB0 -t 9200 -w 8888
```

**Error: `dashboard.html not found`**

```bash
# Copy dashboard to same directory as executable
cp ../gui/dashboard.html .

# Or specify full path in code (not recommended)
```

### Serial Port Not Found

**Find USB-RS485 adapter:**
```bash
# List all serial devices
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/ttyS*

# Watch kernel messages when plugging in
dmesg -w
# Then plug in adapter, you'll see:
# usb 1-1: ch341-uart converter now attached to ttyUSB0

# Check USB devices
lsusb
# Look for CH340 or FTDI
```

**Install CH340 driver (usually built-in):**
```bash
# Check if driver is loaded
lsmod | grep ch341

# If not loaded
sudo modprobe ch341

# Make it load on boot
echo "ch341" | sudo tee -a /etc/modules
```

---

## Cross-Compilation

### For Raspberry Pi (ARM)

**On Ubuntu x64, cross-compile for ARM:**

```bash
# Install cross-compiler
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Create toolchain file
cat > arm-toolchain.cmake << EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# Build
mkdir build-arm && cd build-arm
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake ..
make -j$(nproc)

# Copy to Raspberry Pi
scp orion_proxy pi@raspberrypi.local:~/
```

### For ARM64 (aarch64)

```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Modify toolchain file
sed -i 's/arm-linux-gnueabihf/aarch64-linux-gnu/g' arm-toolchain.cmake

# Build
mkdir build-arm64 && cd build-arm64
cmake -DCMAKE_TOOLCHAIN_FILE=../arm-toolchain.cmake ..
make -j$(nproc)
```

### Static Linking (Portable Binary)

```bash
# Build with static libraries
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
      ..
make -j$(nproc)

# Verify (should show "statically linked")
file orion_proxy
ldd orion_proxy  # Should show minimal dependencies
```

---

## Build Options

### CMake Options

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Debug, Release, RelWithDebInfo | Release | Build configuration |
| `CMAKE_INSTALL_PREFIX` | path | /usr/local | Install location |
| `CMAKE_C_COMPILER` | gcc, clang | gcc | C compiler |

**Examples:**

```bash
# Optimized release build
cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Install to /opt
cmake -DCMAKE_INSTALL_PREFIX=/opt ..

# Use Clang instead of GCC
cmake -DCMAKE_C_COMPILER=clang ..
```

### Compiler Flags

**For maximum optimization:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-O3 -march=native -flto" \
      ..
```

**For small binary size:**
```bash
cmake -DCMAKE_BUILD_TYPE=MinSizeRel \
      -DCMAKE_C_FLAGS="-Os -s" \
      ..
```

---

## Platform-Specific Notes

### Ubuntu 18.04 LTS

- Default CMake is 3.10 (too old)
- Install newer CMake from Kitware repository (see troubleshooting)

### CentOS 7

- Use `cmake3` instead of `cmake`
- GCC 4.8 is default (works, but old)
- Consider installing GCC 7+ from SCL:
  ```bash
  sudo yum install centos-release-scl
  sudo yum install devtoolset-7
  scl enable devtoolset-7 bash
  ```

### Raspberry Pi

- Build time: ~2 minutes on Pi 4, ~10 minutes on Pi 3
- Use `-j2` or `-j4` instead of `-j$(nproc)` to avoid overheating
- Runs perfectly on Pi Zero W (512MB RAM)

### Alpine Linux (musl libc)

```bash
apk add build-base cmake git linux-headers
cmake ..
make -j$(nproc)
```

---

## Packaging for Distribution

### Debian/Ubuntu Package

```bash
# Install packaging tools
sudo apt-get install debhelper dh-make

# Create debian package structure
mkdir -p debian/orion-proxy/usr/local/bin
mkdir -p debian/orion-proxy/usr/local/share/orion-proxy
mkdir -p debian/orion-proxy/DEBIAN

# Copy files
cp orion_proxy debian/orion-proxy/usr/local/bin/
cp ../gui/dashboard.html debian/orion-proxy/usr/local/share/orion-proxy/

# Create control file
cat > debian/orion-proxy/DEBIAN/control << EOF
Package: orion-proxy
Version: 1.0.0
Section: utils
Priority: optional
Architecture: amd64
Depends: libc6
Maintainer: Your Name <your@email.com>
Description: Orion RS-485 Proxy Service
 Connects to Bolid Orion RS-485 security bus and provides
 TCP API and web dashboard for monitoring and control.
EOF

# Build package
dpkg-deb --build debian/orion-proxy
mv debian/orion-proxy.deb orion-proxy_1.0.0_amd64.deb

# Install
sudo dpkg -i orion-proxy_1.0.0_amd64.deb
```

### RPM Package (CentOS/Fedora)

```bash
# Install packaging tools
sudo yum install rpm-build rpmdevtools

# Create RPM structure
rpmdev-setuptree

# Create spec file
cat > ~/rpmbuild/SPECS/orion-proxy.spec << EOF
Name:           orion-proxy
Version:        1.0.0
Release:        1%{?dist}
Summary:        Orion RS-485 Proxy Service
License:        MIT
Source0:        %{name}-%{version}.tar.gz

%description
Orion RS-485 Proxy Service for Bolid security systems

%install
mkdir -p %{buildroot}/usr/local/bin
mkdir -p %{buildroot}/usr/local/share/orion-proxy
install -m 755 orion_proxy %{buildroot}/usr/local/bin/
install -m 644 dashboard.html %{buildroot}/usr/local/share/orion-proxy/

%files
/usr/local/bin/orion_proxy
/usr/local/share/orion-proxy/dashboard.html
EOF

# Build RPM
rpmbuild -ba ~/rpmbuild/SPECS/orion-proxy.spec
```

---

## Performance Tuning

### For High Packet Rate (>1000 pkt/s)

```bash
# Increase serial buffer size
sudo sysctl -w kernel.sched_rt_runtime_us=-1

# Set real-time priority
sudo chrt -f 50 ./orion_proxy -p /dev/ttyUSB0
```

### For Low-Power Devices (Raspberry Pi)

```bash
# Reduce CPU usage
./orion_proxy -p /dev/ttyUSB0 --background

# Disable verbose logging
# (edit config or use -v 0)
```

---

## Quick Reference

```bash
# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run
./orion_proxy -p /dev/ttyUSB0

# Install
sudo make install

# Create service
sudo systemctl enable orion-proxy
sudo systemctl start orion-proxy

# Check logs
journalctl -u orion-proxy -f
```
