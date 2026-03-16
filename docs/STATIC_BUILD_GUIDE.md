# Static Build Guide for Orion Proxy

This guide explains how to build a fully static, portable Linux binary of the Orion Proxy that can run on any Ubuntu/Debian system without compilation or dependency issues.

---

## Why Static Build?

**Problem:** Dynamic binaries depend on specific GLIBC versions. A binary built on Ubuntu 22.04 (GLIBC 2.34+) won't run on Ubuntu 20.04 (GLIBC 2.31).

**Solution:** Static linking embeds all dependencies into the binary, creating a self-contained executable that runs on any x86_64 Linux system.

### Benefits

- ✅ **No GLIBC version dependency** — runs on Ubuntu 18.04, 20.04, 22.04, 24.04+
- ✅ **No compilation needed** on target machine
- ✅ **Single file deployment** — just copy and run
- ✅ **Smaller package** — no need to ship libraries
- ✅ **Predictable behavior** — same binary everywhere

---

## Prerequisites

### Build Environment

You need a Linux environment with:
- GCC compiler
- CMake 3.14+
- Static C library (`libc6-dev`)

**Option 1: WSL Ubuntu (Windows)**
```bash
# Install Ubuntu 22.04 in WSL
wsl --install -d Ubuntu-22.04

# Inside WSL:
sudo apt-get update
sudo apt-get install -y build-essential cmake
```

**Option 2: Native Ubuntu/Debian**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake
```

**Option 3: Docker**
```bash
docker run -it --rm -v "$(pwd):/work" ubuntu:22.04 bash
apt-get update && apt-get install -y build-essential cmake
cd /work
```

---

## Quick Build

### Automated Build Script

The easiest way to build:

```bash
cd pc/orion_proxy
./build-static-linux.sh
```

This script:
1. Checks for required tools
2. Configures CMake with `-DSTATIC_BUILD=ON`
3. Compiles with full static linking
4. Strips debug symbols to reduce size
5. Outputs binary to `dist/orion_proxy`

### Manual Build

If you prefer manual control:

```bash
cd pc/orion_proxy
mkdir build-static && cd build-static

# Configure with static build option
cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release

# Compile
make -j$(nproc)

# Strip to reduce size
strip orion_proxy

# Verify it's static
file orion_proxy
ldd orion_proxy  # Should show "not a dynamic executable"
```

---

## Build Output

### Expected Results

```
orion_proxy: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), 
             statically linked, BuildID[sha1]=..., for GNU/Linux 3.2.0, stripped
```

**Size:** ~1.1 MB (stripped)

**Dependencies:** None — fully self-contained

### Verification

```bash
# Check file type
file dist/orion_proxy
# Output: ELF 64-bit LSB executable, x86-64, statically linked

# Check for dynamic dependencies (should fail)
ldd dist/orion_proxy
# Output: not a dynamic executable

# Test run
./dist/orion_proxy --help
```

---

## CMake Configuration Details

### Static Build Option

The `CMakeLists.txt` includes:

```cmake
option(STATIC_BUILD "Build fully static binary for portability" OFF)
if(STATIC_BUILD AND UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    add_definitions(-DSTATIC_BUILD)
endif()
```

### What `-static` Does

- Links against static versions of all libraries (`.a` files)
- Embeds libc, libpthread, libm into the binary
- No runtime library dependencies
- Larger binary size, but completely portable

### Platform Support

| Platform | Static Build | Notes |
|----------|--------------|-------|
| Linux x86_64 | ✅ Yes | Fully supported |
| Linux ARM64 | ✅ Yes | Cross-compile or native build |
| Linux x86 (32-bit) | ✅ Yes | Use 32-bit build environment |
| macOS | ❌ No | macOS doesn't support full static linking |
| Windows | N/A | Uses static MSVC runtime by default |

---

## Packaging for Distribution

### Create Distributable Package

After building, create a ready-to-deploy package:

```bash
cd pc/orion_proxy
./package-linux.sh
```

This creates: `orion-proxy-1.0.0-linux-x86_64.tar.gz` (~472 KB)

### Package Contents

```
orion-proxy-1.0.0-linux-x86_64/
├── orion_proxy              # Static binary
├── orion_proxy.ini          # Default config
├── gui/
│   └── dashboard.html       # Web dashboard
├── orion-proxy.service      # Systemd service file
├── install.sh               # Automated installer
├── uninstall.sh             # Uninstaller
└── README.txt               # Quick start guide
```

### Deployment

On any Ubuntu/Debian system (no compilation needed):

```bash
# Extract
tar xzf orion-proxy-1.0.0-linux-x86_64.tar.gz
cd orion-proxy-1.0.0-linux-x86_64

# Install
sudo ./install.sh

# Configure
sudo nano /opt/orion-proxy/orion_proxy.ini

# Start
sudo systemctl start orion-proxy
```

---

## Troubleshooting

### Build Fails: "cannot find -lc"

**Problem:** Static libc not installed.

**Solution:**
```bash
sudo apt-get install -y libc6-dev
```

### Build Fails: "cannot find -lpthread"

**Problem:** Static pthread not available.

**Solution:** Already included in `libc6-dev`, but verify:
```bash
ls /usr/lib/x86_64-linux-gnu/libpthread.a
```

### Binary Size Too Large

**Problem:** Unstripped binary is ~3 MB.

**Solution:** Strip debug symbols:
```bash
strip orion_proxy
# Reduces to ~1.1 MB
```

### "GLIBC_X.XX not found" on Target

**Problem:** You built a dynamic binary instead of static.

**Solution:** Verify static build:
```bash
ldd orion_proxy
# Should say "not a dynamic executable"

# If it shows libraries, rebuild with:
cmake .. -DSTATIC_BUILD=ON
```

### Cross-Compilation for ARM

**Build ARM64 binary on x86_64:**

```bash
# Install cross-compiler
sudo apt-get install -y gcc-aarch64-linux-gnu

# Configure for ARM64
cmake .. -DSTATIC_BUILD=ON \
         -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
         -DCMAKE_SYSTEM_NAME=Linux \
         -DCMAKE_SYSTEM_PROCESSOR=aarch64

make
```

---

## Comparison: Static vs Dynamic

| Aspect | Static Build | Dynamic Build |
|--------|--------------|---------------|
| Binary size | ~1.1 MB | ~50 KB |
| Dependencies | None | libc, libpthread, etc. |
| GLIBC version | Any | Must match build system |
| Portability | High | Low |
| Memory usage | Slightly higher | Shared libraries |
| Security updates | Rebuild needed | System updates libs |
| Deployment | Copy & run | May need dependencies |

---

## Build Variants

### Release Build (Optimized)

```bash
cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release
```
- Full optimizations (`-O3`)
- No debug symbols
- Smallest, fastest binary

### Debug Build

```bash
cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Debug
```
- Debug symbols included
- No optimizations
- Larger binary (~3 MB)
- Use with `gdb` for debugging

### MinSizeRel Build

```bash
cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=MinSizeRel
```
- Size optimizations (`-Os`)
- Slightly smaller than Release
- Good for embedded systems

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Build Static Linux Binary

on: [push]

jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: sudo apt-get install -y build-essential cmake
      
      - name: Build static binary
        run: |
          cd pc/orion_proxy
          ./build-static-linux.sh
      
      - name: Create package
        run: |
          cd pc/orion_proxy
          ./package-linux.sh
      
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: orion-proxy-linux
          path: pc/orion_proxy/orion-proxy-*.tar.gz
```

---

## Best Practices

1. **Always verify static linking:**
   ```bash
   ldd orion_proxy  # Should fail or say "not a dynamic executable"
   ```

2. **Test on target OS before deployment:**
   ```bash
   # Copy to Ubuntu 20.04 VM and test
   ./orion_proxy --help
   ```

3. **Strip binaries for production:**
   ```bash
   strip --strip-all orion_proxy
   ```

4. **Include version info:**
   ```bash
   ./orion_proxy --version  # Should show build date, git commit
   ```

5. **Document minimum kernel version:**
   - Static binaries still need compatible kernel
   - Built on Ubuntu 22.04 → requires Linux 3.2.0+
   - Works on all modern distributions

---

## Security Considerations

### Static Linking and Security Updates

**Trade-off:** Static binaries don't benefit from system library security updates.

**Mitigation:**
- Rebuild regularly with latest libc
- Subscribe to security advisories
- Automate builds in CI/CD
- Version and track deployed binaries

### Recommended Update Cycle

- **Critical security fixes:** Rebuild immediately
- **Regular updates:** Monthly rebuild
- **Stable deployments:** Quarterly rebuild

---

## Related Documentation

- [Ubuntu Installation Guide](UBUNTU_INSTALLATION.md) — Deploy the static binary
- [Linux Build Guide](LINUX_BUILD_GUIDE.md) — Dynamic build instructions
- [PC Proxy Guide](PC_PROXY_GUIDE.md) — Usage and configuration
- [Testing Without Hardware](TESTING_WITHOUT_HARDWARE.md) — Test the binary

---

## Summary

**To create a portable Linux binary:**

```bash
# 1. Build static binary
cd pc/orion_proxy
./build-static-linux.sh

# 2. Package for distribution
./package-linux.sh

# 3. Deploy on any Ubuntu/Debian
# Copy orion-proxy-1.0.0-linux-x86_64.tar.gz to target
tar xzf orion-proxy-1.0.0-linux-x86_64.tar.gz
cd orion-proxy-1.0.0-linux-x86_64
sudo ./install.sh
```

**Result:** Self-contained binary that runs on any modern Linux distribution without compilation or dependency issues.
