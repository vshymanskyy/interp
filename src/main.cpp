// Select interpreter mode:
#define USE_DTC             // Direct Threaded Code
//#define USE_TTC           // Token (Indirect) Threaded Code
//#define USE_SWITCH        // Switching
//#define USE_TAIL_CALLS    // Tail Calls
//#define USE_CALLS         // Calls Loop
//#define USE_INLINE        // Machine Code Inlining

#include <Arduino.h> 
#include <stdarg.h>

/* TODO:
 * - Handle printf. Enter/exit inline seq?
 * - Separate halt for each dispatch type
 * - Use bytecode as input and translate?
 * - Forward label references
 * - Get rid of void**
 * - ESP8266 EXEC alloc: info here https://github.com/esp8266/Arduino/pull/7060
 * - ESP32: Align with NOPs without jumping?
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
    DECN(0x100);
    DUP();
    DECN(0x100);
    HALT();

    return vPC;
}

#define LOOP_COUNT 100000

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
    while (i--) {}
}

void interp_task(void*) {
    DBG_PRINTF("Initializing...\n");

    interp_init();

    void** prog = (void**)malloc_exec();
    ASSERT(prog);

    DBG_PRINTF("Generating Code @ %p\n", prog);
    
    void** prog_end = example_1(prog);

    DBG_PRINTF("Code: %zd bytes\n", (char*)prog_end-(char*)prog);

    uint32_t t = micros();
    interp_run(prog);
    uint32_t vm_time = micros() - t;
    Serial.print("Virtual: "); Serial.print(vm_time);    Serial.println(" us");
    
    t = micros();
    native_example_1();
    uint32_t native_time = micros() - t;
    Serial.print("Native:  "); Serial.print(native_time); Serial.println(" us");
   
    Serial.print("Differs: ");  Serial.print(float(vm_time)/native_time); Serial.println("x");

    DBG_PRINTF("Stack: %08zx %08zx %08zx\n", gStack[STACK_SIZE-1], gStack[STACK_SIZE-2], gStack[STACK_SIZE-3]);

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
