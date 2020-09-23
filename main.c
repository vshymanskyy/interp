#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

// Select interpreter mode:
//#define USE_DTC             // Direct Threaded Code
//#define USE_TTC           // Token (Indirect) Threaded Code
//#define USE_SWITCH        // Switching
//#define USE_TAIL_CALLS    // Tail Calls
//#define USE_CALLS         // Calls Loop
#define USE_INLINE        // Machine Code Inlining

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

#define LOOP_COUNT 100000000

void** example_1(void** vPC)
{
    PUSH(LOOP_COUNT);
    LABEL(loop);
        DEC();
        JNZ(loop);
    HALT();

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

    void** prog = (void**)malloc_exec();
    ASSERT(prog);

    DBG_PRINTF("Generating Code @ %p\n", prog);
    
    void** prog_end = example_1(prog);

    DBG_PRINTF("Code: %zd bytes\n", (char*)prog_end-(char*)prog);

    interp_run(prog);
    
    DBG_PRINTF("Stack: %08zx %08zx %08zx\n", gStack[STACK_SIZE-1], gStack[STACK_SIZE-2], gStack[STACK_SIZE-3]);

    return 0;
}
