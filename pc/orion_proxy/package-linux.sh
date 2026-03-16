#!/bin/bash
# Package Orion Proxy into a distributable archive for Ubuntu
# Run after build-static-linux.sh
#
# Usage:   ./package-linux.sh
# Output:  orion-proxy-linux-x64.tar.gz

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="1.0.0"
ARCH="$(uname -m)"
PACKAGE_NAME="orion-proxy-${VERSION}-linux-${ARCH}"
PACKAGE_DIR="$SCRIPT_DIR/$PACKAGE_NAME"
BINARY="$SCRIPT_DIR/dist/orion_proxy"

# Check binary exists
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "Run ./build-static-linux.sh first"
    exit 1
fi

echo "=== Packaging Orion Proxy v${VERSION} ==="

# Clean previous package
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

# Copy binary
cp "$BINARY" "$PACKAGE_DIR/"
chmod +x "$PACKAGE_DIR/orion_proxy"

# Copy dashboard
if [ -d "$SCRIPT_DIR/gui" ]; then
    mkdir -p "$PACKAGE_DIR/gui"
    cp "$SCRIPT_DIR/gui/"* "$PACKAGE_DIR/gui/" 2>/dev/null || true
fi

# Create default config
cat > "$PACKAGE_DIR/orion_proxy.ini" << 'EOF'
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
EOF

# Create systemd service file
cat > "$PACKAGE_DIR/orion-proxy.service" << 'EOF'
[Unit]
Description=Orion RS-485 Proxy Service
After=network.target

[Service]
Type=simple
ExecStart=/opt/orion-proxy/orion_proxy -c /opt/orion-proxy/orion_proxy.ini
WorkingDirectory=/opt/orion-proxy
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Create install script
cat > "$PACKAGE_DIR/install.sh" << 'INSTALL_EOF'
#!/bin/bash
# Orion Proxy Installer for Ubuntu
set -e

INSTALL_DIR="/opt/orion-proxy"
SERVICE_NAME="orion-proxy"

echo ""
echo "  Orion RS-485 Proxy Installer"
echo "  ============================"
echo ""

# Check root
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: Run as root: sudo ./install.sh"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Stop existing service if running
if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
    echo "Stopping existing service..."
    systemctl stop "$SERVICE_NAME"
fi

# Create install directory
echo "Installing to $INSTALL_DIR ..."
mkdir -p "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/gui"

# Copy files
cp "$SCRIPT_DIR/orion_proxy" "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/orion_proxy"

# Copy dashboard
if [ -d "$SCRIPT_DIR/gui" ]; then
    cp -r "$SCRIPT_DIR/gui/"* "$INSTALL_DIR/gui/" 2>/dev/null || true
fi

# Copy config (don't overwrite existing)
if [ ! -f "$INSTALL_DIR/orion_proxy.ini" ]; then
    cp "$SCRIPT_DIR/orion_proxy.ini" "$INSTALL_DIR/"
    echo "Config created: $INSTALL_DIR/orion_proxy.ini"
else
    echo "Config exists, not overwriting: $INSTALL_DIR/orion_proxy.ini"
fi

# Install systemd service
cp "$SCRIPT_DIR/orion-proxy.service" /etc/systemd/system/
systemctl daemon-reload

# Add current user to dialout group for serial port access
REAL_USER="${SUDO_USER:-$USER}"
if ! groups "$REAL_USER" | grep -q dialout; then
    usermod -a -G dialout "$REAL_USER"
    echo "Added $REAL_USER to dialout group (re-login required)"
fi

# Create symlink for easy CLI access
ln -sf "$INSTALL_DIR/orion_proxy" /usr/local/bin/orion_proxy

echo ""
echo "  Installation complete!"
echo ""
echo "  Edit config:    nano $INSTALL_DIR/orion_proxy.ini"
echo "  Start service:  sudo systemctl start orion-proxy"
echo "  Enable on boot: sudo systemctl enable orion-proxy"
echo "  Check status:   sudo systemctl status orion-proxy"
echo "  View logs:      sudo journalctl -u orion-proxy -f"
echo "  Run manually:   orion_proxy -p /dev/ttyUSB0 -v"
echo "  Dashboard:      http://localhost:8080"
echo ""
INSTALL_EOF
chmod +x "$PACKAGE_DIR/install.sh"

# Create uninstall script
cat > "$PACKAGE_DIR/uninstall.sh" << 'UNINSTALL_EOF'
#!/bin/bash
# Orion Proxy Uninstaller
set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: Run as root: sudo ./uninstall.sh"
    exit 1
fi

echo "Uninstalling Orion Proxy..."

# Stop and disable service
systemctl stop orion-proxy 2>/dev/null || true
systemctl disable orion-proxy 2>/dev/null || true
rm -f /etc/systemd/system/orion-proxy.service
systemctl daemon-reload

# Remove files
rm -f /usr/local/bin/orion_proxy
rm -rf /opt/orion-proxy

echo "Orion Proxy uninstalled."
UNINSTALL_EOF
chmod +x "$PACKAGE_DIR/uninstall.sh"

# Create README
cat > "$PACKAGE_DIR/README.txt" << 'README_EOF'
Orion RS-485 Proxy Service
==========================

Quick Start:
  1. sudo ./install.sh
  2. Edit config: sudo nano /opt/orion-proxy/orion_proxy.ini
     - Set your serial port (e.g. /dev/ttyUSB0)
  3. sudo systemctl start orion-proxy
  4. Open http://localhost:8080 in browser

Manual Run (without installing):
  ./orion_proxy -p /dev/ttyUSB0 -v

Help:
  ./orion_proxy --help

Uninstall:
  sudo ./uninstall.sh
README_EOF

# Create tarball
cd "$SCRIPT_DIR"
tar czf "${PACKAGE_NAME}.tar.gz" "$PACKAGE_NAME/"

# Cleanup
rm -rf "$PACKAGE_DIR"

echo ""
echo "=== Package created ==="
echo "File: ${PACKAGE_NAME}.tar.gz"
echo "Size: $(du -h "${PACKAGE_NAME}.tar.gz" | cut -f1)"
echo ""
echo "Deploy to Ubuntu:"
echo "  1. Copy ${PACKAGE_NAME}.tar.gz to target machine"
echo "  2. tar xzf ${PACKAGE_NAME}.tar.gz"
echo "  3. cd ${PACKAGE_NAME}"
echo "  4. sudo ./install.sh"
echo ""
