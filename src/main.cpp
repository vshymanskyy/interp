// Select interpreter mode:
//#define USE_DTC           // Direct Threaded Code
//#define USE_TTC           // Token (Indirect) Threaded Code
//#define USE_SWITCH        // Switching
//#define USE_TAIL_CALLS    // Tail Calls
//#define USE_CALLS         // Calls Loop
#define USE_INLINE          // Machine Code Inlining

#include <Arduino.h> 
#include <stdarg.h>


/* TODO:
 * - Separate halt for each dispatch type
 * - Clean way of assigning IDs for IMMs
 * - Use bytecode as input and translate?
 * - Forward label references
 * - Get rid of void**
 * - ESP8266 EXEC alloc: info here https://github.com/esp8266/Arduino/pull/7060
 * - Clean memcpy32
 * - ESP32: Align with NOPs without jumping?
 * - ESP*: Fix USE_TTC
 */

void DBG_PRINTF(const char* format, ...)
{
  static char buff[128] = { 0, };
  va_list args;
  va_start(args, format);
  vsnprintf(buff, sizeof(buff), format, args);
  va_end(args);
  Serial.print(buff);
}

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
    PUSH(1000);
    LABEL(loop);
        DEC(1);
        DUP();
        JNZ(loop);
    HALT();

    return vPC;
}

void interp_task(void*) {
    DBG_PRINTF("Initializing...\n");

    interp_init();

    void** prog = (void**)malloc_exec();
    ASSERT(prog);

    DBG_PRINTF("Generating Code @ 0x%08x\n", prog);
    
    void** prog_end = example_1(prog);

    DBG_PRINTF("Code: %d bytes\n", (char*)prog_end-(char*)prog);

    delay(10);

    uint32_t t = micros();
    interp_run(prog);
    DBG_PRINTF("Time: %d us\n", micros() - t);
    
    DBG_PRINTF("Stack: %08x %08x %08x\n", gStack[STACK_SIZE-1], gStack[STACK_SIZE-2], gStack[STACK_SIZE-3]);

#ifdef ESP32
    vTaskDelete(NULL);
#endif
}

void setup()
{
    delay(500);
    Serial.begin(115200);
    Serial.print("\n\n");

#ifdef ESP32
    // On ESP32, we can launch in a separate thread
    xTaskCreate(&interp_task, "interp", 32768, NULL, 5, NULL);
#else
    interp_task(NULL);
#endif
}

void loop() {
    delay(100);
}
