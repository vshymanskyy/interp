# Supported Targets, Cross-Compilation & Embedded Devices

This document covers supported CPU architectures, hardware platforms, cross-compilation instructions, and embedded microcontroller deployment.

---

## Supported Architectures & Targets

| Architecture | Word Size | Inlining Support | Tested Platforms & Environments |
| :--- | :---: | :---: | :--- |
| **x86-64** | 64-bit | Yes | Linux, macOS (Intel), Windows (MSVC & Clang) |
| **x86** (i386) | 32-bit | Yes | Linux (`gcc-multilib`), Windows (`-m32`) |
| **AArch64** (ARM64) | 64-bit | Yes | Linux, macOS Apple Silicon (`MAP_JIT`), Windows on ARM |
| **ARM** (armhf / armel) | 32-bit | Yes | Linux (QEMU), Nordic nRF51 / nRF52 (PlatformIO) |
| **RISC-V (RV64)** | 64-bit | Yes | Linux (QEMU), Kendryte K210 dual-core RV64GC (Sipeed MAIX) |
| **RISC-V (RV32)** | 32-bit | Yes | Embedded bare-metal / QEMU |
| **MIPS / MIPSEL** | 32-bit | Yes | Linux Big-Endian & Little-Endian (QEMU) |
| **Xtensa** | 32-bit | Yes | Espressif ESP32 (`MALLOC_CAP_EXEC`), ESP8266 (IRAM scratchpad) |
| **WebAssembly** | 32-bit | DTC fallback | WASI SDK / Wasmtime |

---

## WebAssembly (WASI)

Compile to WebAssembly using [WASI SDK](https://github.com/WebAssembly/wasi-sdk) and execute via [Wasmtime](https://wasmtime.dev/):

```bash
$WASI_SDK_PATH/bin/clang --target=wasm32-wasip1 --sysroot=$WASI_SDK_PATH/share/wasi-sysroot \
  main.c -I./src -Os -o interp.wasm

wasmtime run interp.wasm
```

> **Note:** Because WebAssembly environments cannot mark linear memory as executable without runtime JIT extensions, `interp.h` automatically falls back to `USE_DTC` (Direct Threaded Code).

---

## Cross-Architecture Emulation (QEMU)

Test different CPU architectures on Linux using cross-compilers and `qemu-user-static`:

```bash
sudo apt install qemu-user-static

# ARM 32-bit (armel / armhf)
sudo apt install gcc-arm-linux-gnueabihf libc6-dev-armhf-cross
arm-linux-gnueabihf-gcc -static main.c -I./src -Os -fPIC -o interp-arm
qemu-arm-static ./interp-arm

# AArch64 (ARM 64-bit)
sudo apt install gcc-aarch64-linux-gnu libc6-dev-arm64-cross
aarch64-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-aarch64
qemu-aarch64-static ./interp-aarch64

# RISC-V 64-bit (RV64)
sudo apt install gcc-riscv64-linux-gnu libc6-dev-riscv64-cross
riscv64-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-rv64
qemu-riscv64-static ./interp-rv64

# MIPS (Big Endian)
sudo apt install gcc-mips-linux-gnu libc6-dev-mips-cross
mips-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-mips
qemu-mips-static ./interp-mips

# MIPSEL (Little Endian)
sudo apt install gcc-mipsel-linux-gnu libc6-dev-mipsel-cross
mipsel-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-mipsel
qemu-mipsel-static ./interp-mipsel
```

---

## Embedded Microcontrollers (PlatformIO)

The repository includes a ready-to-flash [`platformio.ini`](../platformio.ini) and Arduino sketch [`src/main.cpp`](../src/main.cpp) comparing VM execution speed against bare-metal C loops.

```bash
# Build and upload for ESP32:
pio run -e ESP32 -t upload && pio device monitor

# Build and upload for ESP8266 (uses 48KB IRAM configuration):
pio run -e ESP8266 -t upload && pio device monitor

# Build for Nordic ARM Cortex-M:
pio run -e TinyBLE       # nRF51822 (Cortex-M0)
pio run -e BLENano2      # nRF52832 (Cortex-M4)

# Build for Kendryte K210 RISC-V:
pio run -e SipeedMAIX    # Dual-Core 64-bit RISC-V
```

### ESP8266 Memory Configuration
ESP8266 builds use `PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48` to trade 16KB of flash cache for IRAM (yielding 48KB of contiguous instruction RAM), providing ample space for dynamically emitted inlined machine code.
