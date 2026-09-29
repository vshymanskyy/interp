#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Select interpreter mode (or pass i.e. -DUSE_SWITCH):
//#define USE_DTC           // Direct Threaded Code
//#define USE_TTC           // Token (Indirect) Threaded Code
//#define USE_SWITCH        // Switching
//#define USE_TAIL_CALLS    // Tail Calls
//#define USE_CALLS         // Calls Loop
#if !defined(USE_DTC) && !defined(USE_TTC) && !defined(USE_SWITCH) && \
    !defined(USE_TAIL_CALLS) && !defined(USE_CALLS) && !defined(USE_INLINE)
#define USE_INLINE          // Machine Code Inlining
#endif

//#define DUMP 1

#define DBG_PRINTF printf

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

void native_example_1()
{
    volatile size_t i = LOOP_COUNT;    
    while (i--) { }
}

int main()
{
    DBG_PRINTF("Initializing...\n");

    //native_example_1();
    //return 0;

    interp_init();

    void** test = (void**)malloc_exec();
    ASSERT(test);

    void** test_entry;
    void** test_end = example_2(test, &test_entry);
    finalize_exec(test, test_end);

    interp_run(test_entry);

    if (gStack[STACK_SIZE-1] != 0xBEEF || gStack[STACK_SIZE-2] != 18) {
        DBG_PRINTF("Self-test FAILED\n");
        return 1;
    }
    memset(gStack, 0, sizeof(gStack));

    void** prog = (void**)malloc_exec();
    ASSERT(prog);

    DBG_PRINTF("Generating Code @ %p\n", prog);
    
    void** prog_end = example_1(prog);
    finalize_exec(prog, prog_end);

    DBG_PRINTF("Code: %td bytes\n", (char*)prog_end-(char*)prog);

    interp_run(prog);
    
    DBG_PRINTF("Stack: %08zx %08zx %08zx\n", gStack[STACK_SIZE-1], gStack[STACK_SIZE-2], gStack[STACK_SIZE-3]);

    return 0;
}
