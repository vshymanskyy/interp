# interp
Testing interpreter dispatch methods: `Switching`, `Direct Threaded Code`, `Indirect Threaded Code`, `Tail-Calls` and `Inlining`.

Supports `x86`, `x86-64`, `arm`, `xtensa` architectures.

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
