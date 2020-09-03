# interp
Testing interpreter [dispatch methods](http://www.cs.toronto.edu/~matz/dissertation/matzDissertation-latex2html/node6.html): `Switching`, `Direct Threaded Code`, `Indirect Threaded Code`, `Tail-Calls` and machine code `Inlining`.

Supports `x86`, `x86-64`, `arm`, `aarch64`, `mips`, `mipsel`, `rv32`, `rv64`, `xtensa` architectures.

## Building on Linux

```bash
clang main.c -I./src -Os -fPIC -o interp
# Or use GCC:
#gcc main.c -I./src -Os -fPIC -o interp

./interp
Generating Code @ 0x7f9b51765000
Code: 166 bytes
Stack: 00000000 00000000 00000000
```

## Building for ESP8266/ESP32/ARM devices
Use PlatformIO to build and upload, i.e.:
```bash
pio run -e ESP32 -t upload && pio device monitor
pio run -e ESP8266 -t upload && pio device monitor
pio run -e TinyBLE -t upload && pio device monitor
```

## Example VM code

```cpp
// === Count down from 1000 to 0
PUSH(1000);     // Push 1000 to VM stack
LABEL(loop);    // Store current code position to label "loop"               <┐
    DEC(1);     // Decrement stack top                                        │
    DUP();      // Duplicate top stack value                                  │
    JNZ(loop);  // Consume stack top. If it's non-zero, jump to "loop" label  ┘
HALT();         // Halt machine
```

## Emulating with QEMU

```bash
sudo apt install qemu-user-static

# ARM
sudo apt install gcc-arm-linux-gnueabihf libc6-dev-armhf-cross
arm-linux-gnueabihf-gcc -static main.c -I./src -Os -fPIC -o interp-arm
qemu-arm-static ./interp-arm

# AARCH64
sudo apt install gcc-aarch64-linux-gnu libc6-dev-arm64-cross
aarch64-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-aarch64
qemu-aarch64-static ./interp-aarch64

# MIPS(EL)
sudo apt-get install gcc-mipsel-linux-gnu libc6-dev-mipsel-cross
mipsel-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-mipsel
qemu-mipsel-static ./interp-mipsel

# MIPS
sudo apt-get install gcc-mips-linux-gnu libc6-dev-mips-cross
mips-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-mips
qemu-mips-static ./interp-mips

# RV64
sudo apt install gcc-riscv64-linux-gnu libc6-dev-riscv64-cross
riscv64-linux-gnu-gcc -static main.c -I./src -Os -fPIC -o interp-rv64
qemu-riscv64-static ./interp-rv64
```
