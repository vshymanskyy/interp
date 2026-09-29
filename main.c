#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// Select interpreter mode (or pass i.e. -DUSE_SWITCH):
//#define USE_DTC           // Direct Threaded Code
//#define USE_TTC           // Token (Indirect) Threaded Code
//#define USE_SWITCH        // Switching
//#define USE_TAIL_CALLS    // Tail Calls
//#define USE_CALLS         // Calls Loop
#if !defined(USE_DTC) && !defined(USE_TTC) && !defined(USE_SWITCH) && \
    !defined(USE_TAIL_CALLS) && !defined(USE_CALLS) && !defined(USE_INLINE)
  #if defined(_MSC_VER) && !defined(__clang__) && (defined(_M_X64) || defined(_M_ARM64))
    #define USE_TAIL_CALLS  // MSVC supports only SWITCH, CALLS, TAIL_CALLS (fastest).
                            // Needs optimization (/O1, /O2): unoptimized, the tail calls grow the stack
  #elif defined(_MSC_VER) && !defined(__clang__)
    #define USE_SWITCH      // 32-bit x86: MSVC can't guarantee the tail calls
  #else
    #define USE_INLINE      // Machine Code Inlining
  #endif
#endif

//#define DUMP 1

#define DBG_PRINTF printf

#define ALLOC_EXEC_PAGE_SIZE 4096   // For the heap allocator (non-INLINE modes)

#include "interp.h"

void** example_0(void** vPC)
{
    PUSH(0x10002000);
    DUP();
    DECN(0x100);
    DUP();
    DECN(0x100);
    HALT();

    return vPC;
}

// Exercises the other ops. Labels can only refer backwards, so the code
// starts after the PRINT+HALT stub, which the final JMP targets
void** example_2(void** vPC, void*** entry)
{
    LABEL(done);
        PRINT();
        HALT();
    *entry = vPC;
        PUSH(0xBEEF);                   // Sentinel
        PUSH(10);  PUSH(5);  ADD();     // 15
        PUSH(4);   MUL();               // 60
        PUSH(6);   SUB();               // 54
        PUSH(3);   DIV();               // 18
        INCN(3);   DECN(2);             // 19
        INC();     DEC();    DEC();     // 18
        PUSH(3);                        // Count down to 0
    LABEL(loop);
        DEC();
        DUP();
        JNZP(loop);
        DROP();
        JMP(done);

    return vPC;
}

// Exercises comparison, bitwise, memory and indirect control flow ops
void** example_3(void** vPC, void*** entry)
{
    // Subroutine: ( x ret -- x*2+1 ret )
    LABEL(twice_plus_one);
        SWAP();
        DUP();     ADD();    INC();
        SWAP();
        RET();
    LABEL(done);
        PRINT();
        HALT();
    *entry = vPC;
        PUSH(0xBEEF);                   // Sentinel
        // Bitwise
        PUSH(0xF0); PUSH(0x3C); AND();  // 0x30
        PUSH(0x0F); OR();               // 0x3F
        PUSH(0x05); XOR();              // 0x3A
        PUSH(2);    SHL();              // 0xE8
        PUSH(3);    SHR();              // 0x1D
        NOT();      NOT();              // 0x1D
        // Comparison
        PUSH(5); PUSH(7); LT();         // 1
        PUSH(5); PUSH(7); GT(); ADD();  // 1
        PUSH(9); PUSH(9); EQ(); ADD();  // 2
        PUSH(9); PUSH(9); NE(); ADD();  // 2
        ADD();                          // 0x1F
        // Memory
        PUSH(0x1234); PUSH(17); STORE();   // Unaligned
        PUSH(0x1AB);  PUSH(3);  STORE8();   // Stores 0xAB
        PUSH(17); LOAD();
        PUSH(3);  LOAD8();  ADD();      // 0x12DF
        ADD();                          // 0x12FE
        // Call, return
        CALL(twice_plus_one);           // 0x25FD
        // Indirect jump, through a table in memory
        PUSH(done); PUSH(32); STORE();
        PUSH(32);   LOAD();   JMPI();

    return vPC;
}

typedef void** (*TestGen)(void** vPC, void*** entry);

// Runs a test program, which should leave [0xBEEF, expected] on the stack
static bool run_test(const char* name, TestGen gen, size_t expected)
{
    void** code = (void**)malloc_exec();
    ASSERT(code);

    void** entry;
    void** end = gen(code, &entry);
    finalize_exec(code, end);

    memset(gStack, 0, sizeof(gStack));
    interp_run(entry);

    if (gStack[STACK_SIZE-1] != 0xBEEF || gStack[STACK_SIZE-2] != expected) {
        DBG_PRINTF("Self-test %s FAILED\n", name);
        return false;
    }
    return true;
}

#define LOOP_COUNT (100*1000000)

void** example_1(void** vPC)
{
    PUSH(LOOP_COUNT);
    LABEL(loop);
        EMIT_OP(dummy1);
        EMIT_OP(dummy2);
        EMIT_OP(dummy1);
        EMIT_OP(dummy3);
        EMIT_OP(dummy1);
        EMIT_OP(dummy4);
        EMIT_OP(dummy1);
        EMIT_OP(dummy5);
        DEC();
        JNZ(loop);
    HALT();

    DBG_PRINTF("Instructions: %d\n", LOOP_COUNT*10);

    return vPC;
}

// The same loop as example_1, in C: the native reference
void native_example_1()
{
    volatile size_t i = LOOP_COUNT;
    while (i--) {
        ASM_NOP(1); ASM_NOP(2); ASM_NOP(1); ASM_NOP(3);
        ASM_NOP(1); ASM_NOP(4); ASM_NOP(1); ASM_NOP(5);
    }
}

static double now_ms(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

int main()
{
    DBG_PRINTF("Initializing...\n");

    interp_init();

    if (!run_test("example_2", example_2, 18) ||
        !run_test("example_3", example_3, 0x25FD)) {
        return 1;
    }
    memset(gStack, 0, sizeof(gStack));

    void** prog = (void**)malloc_exec();
    ASSERT(prog);

    DBG_PRINTF("Generating Code @ %p\n", prog);
    
    void** prog_end = example_1(prog);
    finalize_exec(prog, prog_end);

    DBG_PRINTF("Code: %td bytes\n", (char*)prog_end-(char*)prog);

    double t = now_ms();
    interp_run(prog);
    double vm_time = now_ms() - t;

    t = now_ms();
    native_example_1();
    double native_time = now_ms() - t;

    DBG_PRINTF("Time: %.1f ms (native: %.1f ms)\n", vm_time, native_time);
    DBG_PRINTF("Stack: %08zx %08zx %08zx\n", gStack[STACK_SIZE-1], gStack[STACK_SIZE-2], gStack[STACK_SIZE-3]);

    return 0;
}
