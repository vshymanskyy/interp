#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

//#define USE_DTC
//#define USE_TTC
//#define USE_SWITCH
//#define USE_TAIL_CALLS
//#define USE_CALLS
#define USE_INLINE

#define DBG_PRINTF printf

#include "interp.h"

void** example_0(void** vPC)
{
    PUSH(0x10002000);
    DUP();
    DEC(0x100);
    DUP();
    DEC(0x100);
    HALT();

    return vPC;
}

void** example_1(void** vPC)
{
    PUSH(1000000000);
    LABEL(loop);
        DEC(1);
        DUP();
        JNZ(loop);
    HALT();

    return vPC;
}

int main()
{
    DBG_PRINTF("Initializing...\n");

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
