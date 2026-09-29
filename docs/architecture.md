# Virtual Machine Architecture & Instruction Set

This document details the execution model, memory layout, and complete instruction set reference for **interp**.

---

## Evaluation Stack Model

The virtual machine operates on an evaluation stack `gStack` allocated statically with `STACK_SIZE = 256` elements:
- The stack pointer `vSP` starts at `&gStack[STACK_SIZE - 1]` and grows **downward**.
- `vSP` points to the next available empty slot.
- `vSP[1]` represents the top of the stack (TOS).
- `vSP[2]` represents the second item on the stack (NOS).

```
High Memory   ┌───────────────────────────┐
              │ gStack[STACK_SIZE - 1]    │ <── Stack Base
              ├───────────────────────────┤
              │ gStack[STACK_SIZE - 2]    │
              ├───────────────────────────┤
              │ ...                       │
              ├───────────────────────────┤
              │ vSP[2] (Second on Stack)  │
              ├───────────────────────────┤
              │ vSP[1] (Top of Stack)     │
              ├───────────────────────────┤
Low Memory    │ vSP (Next empty slot)     │ <── Stack Pointer (grows downward)
              └───────────────────────────┘
```

---

## Linear Memory Model

In addition to the evaluation stack, the VM provides a dedicated linear data memory buffer `gMemory`:
- **Capacity**: Configurable via `-DMEMORY_SIZE=<bytes>` (defaults to 16 KB). Allocated as a `uint8_t` array with 8-byte alignment.
- **Addressing**: Instructions pass `gMemory` as the immediate base pointer and consume the byte offset from the stack.
- **Word Access**: `LOAD()` and `STORE()` read and write native `size_t` words (`[addr] -> [word]` and `[value, addr] -> []`). Word accesses are implemented via `memcpy` / packed accessors, supporting unaligned offsets across all architectures.
- **Byte Access**: `LOAD8()` and `STORE8()` read and write individual 8-bit unsigned bytes (`[addr] -> [byte]` and `[value, addr] -> []`) at any byte offset.
- **Bounds Checking**: Out-of-range offsets fail an assertion in all modes except Machine Code Inlining, where the checks are compiled out for maximum performance.

---

## Instruction Set Reference

### Stack Manipulation
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `PUSH(imm)` | `( -- imm )` | Pushes an immediate constant onto the stack |
| `DUP()` | `( a -- a a )` | Duplicates the top value on the stack |
| `DROP()` | `( a -- )` | Discards the top value from the stack |
| `SWAP()` | `( a b -- b a )` | Swaps the top two values on the stack |

### Arithmetic
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `ADD()` | `( a b -- a+b )` | Adds the top two values |
| `SUB()` | `( a b -- a-b )` | Subtracts the top value from the second value (`a - b`) |
| `MUL()` | `( a b -- a*b )` | Multiplies the top two values |
| `DIV()` | `( a b -- a/b )` | Divides `a` by `b` (invokes `interp_div` helper for software divide compatibility) |
| `INC()` | `( a -- a+1 )` | Increments the top value by 1 |
| `DEC()` | `( a -- a-1 )` | Decrements the top value by 1 |
| `INCN(imm)` | `( a -- a+imm )` | Increments the top value by an immediate constant |
| `DECN(imm)` | `( a -- a-imm )` | Decrements the top value by an immediate constant |

### Bitwise & Logic
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `AND()` | `( a b -- a&b )` | Bitwise AND of top two values |
| `OR()` | `( a b -- a\|b )` | Bitwise OR of top two values |
| `XOR()` | `( a b -- a^b )` | Bitwise XOR of top two values |
| `NOT()` | `( a -- ~a )` | Bitwise bit inversion of top value |
| `SHL()` | `( a b -- a<<b )` | Bitwise shift left (`a` shifted by `b` mod word size) |
| `SHR()` | `( a b -- a>>b )` | Bitwise shift right (`a` shifted by `b` mod word size) |

### Comparisons
*Comparisons evaluate unsigned values and push `1` (true) or `0` (false).*

| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `EQ()` | `( a b -- a==b )` | Tests equality (`a == b`) |
| `NE()` | `( a b -- a!=b )` | Tests inequality (`a != b`) |
| `LT()` | `( a b -- a<b )` | Tests less-than (`a < b`, unsigned) |
| `GT()` | `( a b -- a>b )` | Tests greater-than (`a > b`, unsigned) |

### Control Flow & Subroutines
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `LABEL(name)` | `( -- )` | Captures current code position into a backward-jump label pointer |
| `JMP(lbl)` | `( -- )` | Unconditional jump to target code label |
| `JMPI()` | `( addr -- )` | Indirect jump to the address popped from the top of the stack |
| `JNZ(lbl)` | `( cond -- cond )` | Jumps to label if top of stack is non-zero (retains condition on stack) |
| `JNZP(lbl)` | `( cond -- )` | Jumps to label if non-zero and pops condition from stack |
| `CALL(lbl)` | `( -- ret_addr )` | Pushes calculated return address and jumps to subroutine `lbl` |
| `RET()` | `( ret_addr -- )` | Returns to the address on top of the stack (alias for `JMPI()`) |
| `HALT()` | `( -- )` | Terminates VM execution and exits to host |

### Memory Access
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `LOAD()` | `( addr -- value )` | Loads a native word from `gMemory + addr` (unaligned-safe) |
| `STORE()` | `( value addr -- )` | Stores a native word into `gMemory + addr` (unaligned-safe) |
| `LOAD8()` | `( addr -- byte )` | Loads an 8-bit unsigned byte from `gMemory + addr` |
| `STORE8()` | `( value addr -- )` | Stores an 8-bit byte into `gMemory + addr` |

### Debugging & I/O
| Macro / Instruction | Stack Effect `( before -- after )` | Description |
| :--- | :---: | :--- |
| `PRINT()` | `( -- )` | Prints current stack contents to standard output |

---

## Advanced Example: Subroutines, Memory & Jump Tables

Demonstrating `CALL`/`RET`, `SWAP`, linear memory operations, and indirect jump tables (`JMPI`):

```c
#include <stdio.h>
#include "interp.h"

void** example_advanced(void** vPC, void*** entry)
{
    // Subroutine: computes x * 2 + 1, preserving return address
    // Stack transition: ( x ret -- x*2+1 ret )
    LABEL(twice_plus_one);
        SWAP();
        DUP();  ADD();  INC();
        SWAP();
        RET();

    LABEL(done);
        PRINT();
        HALT();

    *entry = vPC;
        PUSH(0xBEEF);                   // Sentinel on stack

        // 1. Bitwise operations
        PUSH(0xF0); PUSH(0x3C); AND();  // 0x30
        PUSH(0x0F); OR();               // 0x3F
        PUSH(0x05); XOR();              // 0x3A
        PUSH(2);    SHL();              // 0xE8
        PUSH(3);    SHR();              // 0x1D

        // 2. Comparisons
        PUSH(5); PUSH(7); LT();         // 1 (true)
        ADD();                          // 0x1E

        // 3. Memory store and load
        PUSH(0x1234); PUSH(16); STORE();      // Store word 0x1234 at offset 16
        PUSH(0x1AB);  PUSH(3);  STORE8();     // Store byte 0xAB at offset 3
        PUSH(16); LOAD();                     // Load word from offset 16 (0x1234)
        PUSH(3);  LOAD8(); ADD();             // Load byte (0xAB) + 0x1234 = 0x12DF
        ADD();

        // 4. Subroutine call and return
        CALL(twice_plus_one);

        // 5. Indirect jump through a dispatch table in memory
        PUSH(done); PUSH(32); STORE();        // Write address of 'done' into table
        PUSH(32);   LOAD();   JMPI();         // Read address and jump indirectly

    return vPC;
}
```
