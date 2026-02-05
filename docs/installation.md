# Sunny Installation Guide

**Document ID:** SUNNY-INSTALL-001
**Version:** 1.0
**Date:** February 2026

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Python Installation](#2-python-installation)
3. [C++ Backend Build](#3-cpp-backend-build)
4. [Max/MSP Externals](#4-maxmsp-externals)
5. [RNBO Patchers](#5-rnbo-patchers)
6. [Ableton Remote Script](#6-ableton-remote-script)
7. [Environment Configuration](#7-environment-configuration)
8. [Verification](#8-verification)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Prerequisites

### System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| OS | macOS 12+, Windows 10+, Linux (Ubuntu 22.04+) | macOS 14+, Windows 11 |
| Python | 3.11 | 3.12 |
| RAM | 4 GB | 8 GB |
| Disk | 500 MB | 1 GB |

### Development Tools

| Tool | Version | Purpose |
|------|---------|---------|
| Python | 3.11+ | Runtime and package management |
| CMake | 3.20+ | C++ build system |
| C++ Compiler | GCC 11+, Clang 14+, or MSVC 2022 | Native backend |
| Git | 2.x | Version control |

### Optional Tools

| Tool | Purpose |
|------|---------|
| Max 8 | Max/MSP externals |
| Max SDK | Building Max externals |
| Ableton Live 11+ | DAW integration |

---

## 2. Python Installation

### 2.1 Clone Repository

```bash
git clone https://github.com/your-org/sunny.git
cd sunny
```

### 2.2 Create Virtual Environment

```bash
# Using venv
python3 -m venv .venv
source .venv/bin/activate  # macOS/Linux
# or
.venv\Scripts\activate     # Windows

# Using conda
conda create -n sunny python=3.12
conda activate sunny
```

### 2.3 Install Package

```bash
# Development installation (editable)
pip install -e ".[dev]"

# Production installation
pip install .
```

### 2.4 Verify Installation

```bash
python -c "import sunny; print(sunny.__version__)"
```

---

## 3. C++ Backend Build

The C++ backend provides significant performance improvements for theory computation.

### 3.1 Prerequisites

**macOS:**
```bash
# Install Xcode command line tools
xcode-select --install

# Install CMake via Homebrew
brew install cmake
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake python3-dev
```

**Windows:**
- Install Visual Studio 2022 with C++ workload
- Install CMake from https://cmake.org/download/

### 3.2 Build Native Module

```bash
cd cpp
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run tests
ctest --output-on-failure

# Install Python module
cmake --install . --prefix ~/.local
```

### 3.3 Verify Native Backend

```bash
python -c "
from sunny.core.native_backend import is_native_available, get_backend_info
print(f'Native available: {is_native_available()}')
print(get_backend_info())
"
```

Expected output:
```
Native available: True
{'native_available': True, 'native_version': '0.1.0', 'theory_backend': 'native', 'harmony_backend': 'native'}
```

---

## 4. Max/MSP Externals

### 4.1 Download Max SDK

1. Download Max SDK from https://cycling74.com/downloads/sdk
2. Extract to a known location (e.g., `~/Documents/max-sdk`)
3. Set environment variable:
   ```bash
   export MAX_SDK_PATH=~/Documents/max-sdk
   ```

### 4.2 Build Externals

```bash
cd max
mkdir build && cd build

# Configure with SDK path
cmake .. -DMAX_SDK_PATH=$MAX_SDK_PATH

# Build
cmake --build .
```

### 4.3 Install to Max Packages

```bash
# macOS
cmake --install . --prefix ~/Documents/Max\ 8/Packages/Sunny

# Windows
cmake --install . --prefix "%USERPROFILE%\Documents\Max 8\Packages\Sunny"
```

### 4.4 Verify in Max

1. Launch Max 8
2. Create new patcher
3. Create object: `sunny.euclidean~`
4. Should load without errors

---

## 5. RNBO Patchers

### 5.1 Install RNBO

1. Open Max 8
2. Go to Package Manager
3. Install "RNBO" package

### 5.2 Copy RNBO Patchers

```bash
# macOS
cp rnbo/*.rnbopat ~/Documents/Max\ 8/Packages/Sunny/patchers/

# Windows
copy rnbo\*.rnbopat "%USERPROFILE%\Documents\Max 8\Packages\Sunny\patchers\"
```

### 5.3 Using RNBO Patchers

1. In Max, create object: `rnbo~`
2. Double-click to open RNBO patcher
3. Load sunny.rhythm.rnbopat
4. Connect clock signal to inlet 1
5. Connect outlet 1 to trigger destination

---

## 6. Ableton Remote Script

### 6.1 Locate Remote Scripts Folder

**macOS:**
```
/Applications/Ableton Live 11 Suite.app/Contents/App-Resources/MIDI Remote Scripts/
```

**Windows:**
```
C:\ProgramData\Ableton\Live 11 Suite\Resources\MIDI Remote Scripts\
```

### 6.2 Install Remote Script

```bash
# Create Sunny folder in Remote Scripts
mkdir -p "$REMOTE_SCRIPTS_PATH/Sunny"

# Copy remote script files
cp -r remote_script/* "$REMOTE_SCRIPTS_PATH/Sunny/"
```

### 6.3 Configure in Ableton

1. Launch Ableton Live
2. Open Preferences → Link/Tempo/MIDI
3. In Control Surface dropdown, select "Sunny"
4. Leave Input and Output as "None"

---

## 7. Environment Configuration

### 7.1 Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SUNNY_TCP_PORT` | 9876 | TCP port for Ableton communication |
| `SUNNY_UDP_PORT` | 9877 | UDP port for OSC messages |
| `SUNNY_LOG_LEVEL` | INFO | Logging level (DEBUG, INFO, WARNING, ERROR) |
| `MAX_SDK_PATH` | (none) | Path to Max SDK for building externals |

### 7.2 Configuration File

Create `~/.sunny/config.toml`:

```toml
[connection]
tcp_port = 9876
udp_port = 9877
host = "127.0.0.1"
timeout = 5.0

[logging]
level = "INFO"
file = "~/.sunny/sunny.log"

[backend]
prefer_native = true
```

---

## 8. Verification

### 8.1 Run Test Suite

```bash
# Python tests
pytest tests/ -v

# With coverage
pytest tests/ -v --cov=src/sunny --cov-report=html

# C++ tests
cd cpp/build && ctest --output-on-failure
```

### 8.2 Verify MCP Server

```bash
# Start MCP server
python -m sunny.host.main

# In another terminal, verify with Claude Code or MCP client
```

### 8.3 Verify Ableton Connection

1. Ensure Remote Script is installed
2. Launch Ableton Live
3. Run verification script:
   ```bash
   python -c "
   import asyncio
   from sunny.infrastructure.transport import AbletonConnection

   async def test():
       conn = AbletonConnection()
       if await conn.connect():
           info = await conn.send_command('get_session_info')
           print(f'Connected to Ableton: tempo={info.get(\"tempo\")} BPM')
       else:
           print('Could not connect (offline mode)')

   asyncio.run(test())
   "
   ```

---

## 9. Troubleshooting

### 9.1 Python Import Errors

**Problem:** `ModuleNotFoundError: No module named 'sunny'`

**Solution:**
```bash
# Ensure package is installed
pip install -e .

# Verify installation
pip show sunny
```

### 9.2 Native Backend Not Loading

**Problem:** `Native available: False`

**Solution:**
```bash
# Rebuild with verbose output
cd cpp/build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Check for missing dependencies
ldd sunny_native*.so  # Linux
otool -L sunny_native*.so  # macOS
```

### 9.3 Max Externals Not Loading

**Problem:** Object `sunny.euclidean~` not found

**Solution:**
1. Verify externals were built:
   ```bash
   ls max/build/*.mxo  # macOS
   ls max/build/*.mxe64  # Windows
   ```
2. Check Max console for error messages
3. Verify file is in correct Packages folder

### 9.4 Ableton Connection Failed

**Problem:** `TCP connection failed: Connection refused`

**Solution:**
1. Verify Remote Script is installed correctly
2. Restart Ableton Live
3. Check ports are not blocked:
   ```bash
   lsof -i :9876  # macOS/Linux
   netstat -an | find "9876"  # Windows
   ```

### 9.5 CMake Configuration Errors

**Problem:** `Could not find pybind11`

**Solution:**
```bash
# pybind11 is fetched automatically, but if it fails:
pip install pybind11

# Then reconfigure with pybind11 path
cmake .. -Dpybind11_DIR=$(python -c "import pybind11; print(pybind11.get_cmake_dir())")
```

---

## Quick Start Summary

```bash
# 1. Clone and install Python package
git clone https://github.com/your-org/sunny.git
cd sunny
python3 -m venv .venv && source .venv/bin/activate
pip install -e ".[dev]"

# 2. Build native backend (optional but recommended)
cd cpp && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ../..

# 3. Run tests
pytest tests/ -v

# 4. Start MCP server
python -m sunny.host.main
```
