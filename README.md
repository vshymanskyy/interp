# interp

[![tests](https://github.com/vshymanskyy/interp/actions/workflows/tests.yml/badge.svg)](https://github.com/vshymanskyy/interp/actions/workflows/tests.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/C%20Standard-C99%20%2F%20C11-informational.svg)](main.c)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32%20%7C%20ESP8266%20%7C%20nRF5x%20%7C%20K210-orange.svg)](platformio.ini)

**interp** is a lightweight, zero-dependency C/C++ virtual machine and research harness for studying, comparing, and benchmarking interpreter [dispatch methods](http://www.cs.toronto.edu/~matz/dissertation/matzDissertation-latex2html/node6.html):
- **Machine Code Inlining** - Stitches precompiled native instructions directly into an executable buffer and patches operands in-place for near-native execution speed.
- **Direct Threaded Code (DTC)** - Jumps directly between opcode handler addresses embedded in the instruction stream, bypassing a central dispatch loop.
- **Indirect / Token Threaded Code (TTC)** - Dispatches opcodes through a lookup table using compact tokens, keeping bytecode small on 64-bit systems.
- **Tail-Call Dispatch** - Chains opcodes as functions that tail-call the next handler without growing the call stack.
- **Calls Loop** - Runs an unrolled trampoline loop where each opcode function returns the pointer to the next handler.
- **Switch-Based Dispatch** - Uses a classic switch statement inside a central loop; simple, baseline, and universally portable.

It runs across desktop, mobile, server, WebAssembly, and bare-metal microcontroller environments - from x86-64 and Apple Silicon to Xtensa ESP32/ESP8266 and RISC-V Kendryte K210.

---

## Overview

Interpreter dispatch overhead is often the primary bottleneck in bytecode execution. Traditional `switch`-based interpreters suffer from shared indirect branch prediction penalties, while direct threaded code eliminates the centralized switch table at the expense of portability.

**interp** implements and evaluates six distinct dispatch mechanisms within a single unified codebase, including an ultra-low-overhead **Machine Code Inlining** engine. Rather than interpreting bytecodes at runtime, the inlining engine copies precompiled native instruction snippets into an executable buffer, patches immediate values and branch offsets in-memory, flushes the CPU instruction cache, and jumps straight into the synthesized native code.

> **Compiler Notes:**
> - **MSVC** does not support GNU computed gotos (`&&label`) or machine code inlining. Selecting `USE_INLINE`, `USE_DTC`, or `USE_TTC` with MSVC fails the build; `main.c` defaults to `USE_SWITCH` on MSVC.
> - **WebAssembly (WASI)** environments cannot mark linear memory as executable without runtime JIT extensions; `interp.h` automatically falls back to `USE_DTC`.

---

## Quick Example

A countdown loop verifying arithmetic and conditional jumps:

```c
#include <stdio.h>
#include "interp.h"

// Count down from 1000 to 0
void** example_countdown(void** vPC)
{
    PUSH(1000);     // Push 1000 onto stack
    LABEL(loop);    // Mark loop destination           <┐
        DEC();      // Decrement stack top              │
        JNZ(loop);  // If non-zero, loop back          ─┘
    HALT();
    return vPC;
}
```

---

## Building & Running

### Linux (x86_64, x86)

```bash
# Default mode (Machine Code Inlining):
clang main.c -I./src -Os -fPIC -o interp
./interp

# Or build with GCC:
gcc main.c -I./src -Os -fPIC -o interp
./interp

# 32-bit x86 build (requires gcc-multilib):
gcc main.c -I./src -Os -fPIC -m32 -o interp-x86
./interp-x86

# Select an alternative dispatch method:
gcc main.c -I./src -Os -fPIC -DUSE_DTC -o interp-dtc
gcc main.c -I./src -Os -fPIC -DUSE_TTC -o interp-ttc
gcc main.c -I./src -Os -fPIC -DUSE_SWITCH -o interp-switch
gcc main.c -I./src -Os -fPIC -DUSE_TAIL_CALLS -o interp-tailcalls
gcc main.c -I./src -Os -fPIC -DUSE_CALLS -o interp-calls
```

### macOS (Apple Silicon & Intel)

On macOS Apple Silicon (ARM64), `interp` automatically coordinates with Darwin's JIT security model via `MAP_JIT`, `pthread_jit_write_protect_np()`, and `sys_icache_invalidate()`:

```bash
clang main.c -I./src -Os -fPIC -o interp
./interp
```

### Windows (Clang & MSVC)

#### Using Clang (LLVM):
```cmd
clang main.c -I./src -Os -o interp.exe
interp.exe
```

#### Using Microsoft Visual C++ (`cl.exe`):
MSVC supports `USE_SWITCH` (the default), `USE_CALLS`, and `USE_TAIL_CALLS`:
```cmd
cl /nologo /O2 /std:c11 /W3 /I src /DUSE_SWITCH main.c /Fe:interp.exe
interp.exe
```

---

## Self-Tests & Benchmarks

The test runner in [`main.c`](main.c) executes a multi-stage validation suite upon launch:
- **`example_2`**: Arithmetic regression test validating label references, countdown loops (`DEC`, `DUP`, `JNZP`), and stack unwinding. Expected stack result: `18`.
- **`example_3`**: Comprehensive feature verification covering bitwise operations, unsigned comparisons, linear memory operations (`STORE`, `LOAD`, `STORE8`, `LOAD8`), subroutine calls (`CALL`/`RET`), and indirect memory-backed jumps (`JMPI`). Expected stack result: `0x25FD`.
- **`example_1`**: Performance benchmark running a 100-million iteration loop with dummy NOP instructions to measure raw dispatch throughput.

```
Initializing...
Generating Code @ 0x7f354ab9c000
Code: 84 bytes
Stack: 00000000 00000000 00000000
```

---

## Documentation

For in-depth guides and architectural references, see the [`docs/`](docs/) directory:

- [**Virtual Machine Architecture & Instruction Set**](docs/architecture.md) — Evaluation stack model, linear memory layout (`gMemory`), complete opcode reference tables, and advanced subroutine examples.
- [**How Machine Code Inlining Works**](docs/inlining.md) — JIT template extraction, dynamic chunk emission, immediate placeholder patching, memory mapping (`mmap`/`VirtualAlloc`/IRAM), cache coherence, and architecture flowcharts.
- [**Supported Targets & Cross-Compilation**](docs/targets.md) — CPU architecture matrix, cross-compilation with QEMU (ARM, AArch64, RISC-V, MIPS), WebAssembly (WASI), and embedded deployment with PlatformIO (ESP32, ESP8266, nRF5x, K210).

---

## References

- Matz, N. (2003). *Design and Implementation of Interpreter Dispatch Techniques*. [University of Toronto Dissertation](http://www.cs.toronto.edu/~matz/dissertation/matzDissertation-latex2html/node6.html).
- Piumarta, I., & Riccardi, F. (1998). *Optimizing direct code with bytecodes and machine-independent dynamic compilation*. ACM SIGPLAN Notices.
- Ertl, M. A., & Gregg, D. (2003). *The Structure and Performance of Efficient Interpreters*. Journal of Instruction-Level Parallelism.

---

## License

This project is licensed under the [MIT License](LICENSE) - see the LICENSE file for details.

Copyright (c) 2020 Volodymyr Shymanskyy.
