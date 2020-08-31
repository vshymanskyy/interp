#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "interp_utils.h"
#include "opcodes_enum.h"

#define STACK_SIZE 256
static size_t gStack[STACK_SIZE] = { (size_t)-1, };

// Syntactic Helpers

#define LABEL(lbl)                  void** lbl = vPC;
#define PUSH(value)                 EMIT_OP_IMM(push,value);
#define JNZ(value)                  EMIT_OP_IMM(jnz,value);
#define DUP()                       EMIT_OP(dup);
#define INC(value)                  EMIT_OP_IMM(inc,value);
#define DEC(value)                  EMIT_OP_IMM(dec,value);
#define HALT()                      EMIT_OP(halt);


#if !defined(USE_INLINE)

    #define EMIT_OP_IMM(op,imm)     { EMIT_OP(op); *vPC++ = (void*)(imm); }

    #define GET_IMM(op)             register size_t imm = (size_t)*vPC++;
    #define JUMP(addr)              { vPC = (void**)(addr); NEXT(); }

#endif


#if defined(USE_DTC) || defined(USE_TTC)

    #if defined(USE_DTC)            // Direct Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         *vPC++ = gLabelTable[op_##op]
        #define NEXT()              goto **(vPC++)
    #elif defined(USE_TTC)          // Indirect (Token) Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         *(uint8_t*)vPC++ = (uint8_t)op_##op
        #define NEXT()              goto *gLabelTable[*(uint8_t*)vPC++]
    #endif

    static void* gLabelTable[Opcode_qty];

    static inline
    int interp_run(void** prog)
    {
        register void** vPC = prog;
        register size_t* vSP = &gStack[STACK_SIZE-1];
        if (prog) {
            NEXT();
        }

        #define STORE_OP(label) gLabelTable[op_##label] = &&label_##label
        STORE_OP(push);
        STORE_OP(drop);
        STORE_OP(dup);
        STORE_OP(inc);
        STORE_OP(dec);
        STORE_OP(add);
        STORE_OP(sub);
        STORE_OP(print);
        STORE_OP(jmp);
        STORE_OP(jnz);
        STORE_OP(halt);
        #undef STORE_OP
        return 0;

        #include "opcodes_impl.h"
    }

#elif defined(USE_SWITCH)

    #define OP(op, code)        case op_##op: { code; } NEXT();
    #define EMIT_OP(op)         *vPC++ = (void*)op_##op
    #define NEXT()              continue;

    static inline
    int interp_run(void** prog)
    {
        if (!prog) return 0;
        
        register void** vPC = prog;
        register size_t* vSP = &gStack[STACK_SIZE-1];

        while (true) {
            switch ((Opcode)(size_t)*vPC++) {
                #include "opcodes_impl.h"
                default: return 1;
            }
        }
    }
    
#elif defined(USE_TAIL_CALLS)

    #define OpSig               void** vPC, size_t* vSP

    #define OP(op, code)        int opf_##op(OpSig) { { code; } NEXT(); }
    #define EMIT_OP(op)         *vPC++ = (void*)opf_##op
    #define NEXT()              return ((OpFunc)(* vPC))(vPC + 1, vSP);

    typedef int (*OpFunc)(OpSig);

    #include "opcodes_impl.h"

    static inline
    int interp_run(void** prog)
    {
        if (!prog) return 0;

        register void** vPC = prog;
        register size_t* vSP = &gStack[STACK_SIZE-1];

        NEXT();
    }

#elif defined(USE_CALLS)

    #define OP(op, code)        int opf_##op() { { code; } NEXT(); }
    #define EMIT_OP(op)         *vPC++ = (void*)opf_##op
    #define NEXT()              return CONTINUE;

    #define CONTINUE            0xdeadbeef

    typedef int (* OpFunc)();
    
    void** vPC;
    size_t* vSP = &gStack[STACK_SIZE-1];

    #include "opcodes_impl.h"

    static inline
    int interp_run(void** prog)
    {
        if (!prog) return 0;

        vPC = prog;

        for(;;) {
            int res = ((OpFunc)(* vPC++))();
            if (res != CONTINUE) return res;
        }
    }

#elif defined(USE_INLINE)

    #define GET_IMM(op)         ASM_GET_IMM(STRINGIFY(op))

    #if UINTPTR_MAX == 0xFFFFFFFF
        #define PLACEHOLDER     0xdeadbeef
    #elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
        #define PLACEHOLDER     0xdeadbeefdeadbeef
    #else
        #error "Bogus pointer size"
    #endif

    #if defined(__xtensa__)
        // Store opcodes in IRAM for Xtensa devices.
        // Use 32-bit (aligned) operations to access the memory.
        // TODO/OPTZ: Probably can read from flash memory?
        #define TEXT_SECTION                IRAM_ATTR
        #define TEXT_READ(dst,src,len)      ASSERT(IS_OP_ALIGNED(src)); memcpy32(dst,src,len) // ESP.flashRead((uint32_t)src,(uint32_t*)dst,len);
        #define TEXT_WRITE(dst,src,len)     ASSERT(IS_OP_ALIGNED(dst)); memcpy32(dst,src,len)
    #else
        #define TEXT_SECTION
        #define TEXT_READ(dst,src,len)      ASSERT(IS_OP_ALIGNED(src)); memcpy(dst,src,len);
        #define TEXT_WRITE(dst,src,len)     ASSERT(IS_OP_ALIGNED(dst)); memcpy(dst,src,len);
    #endif

    #if defined(__x86_64__) || defined(__i386__)
        #define ASM_GUARD(code) asm(""); code; asm("");
        #define ASM_GET_IMM(ID) register size_t imm;                                            \
                                asm volatile("mov $(" STRINGIFY(PLACEHOLDER) " + " ID "), %0"   \
                                             : "=r"(imm));
        #define JUMP(addr)      asm volatile("jmp *%0" : : "r"(addr));
    #elif defined(__arm__)
        #define OP_ALIGN        4
        #define ASM_GUARD(code) while(dummy) { code; asm(""); }
        #define ASM_GET_IMM(ID) register size_t imm;                                            \
                                asm volatile("ldr %0, [pc, #0]"                             EOL \
                                             "b   .CONT_" ID                                EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + " ID ")"  EOL \
                                             ".CONT_" ID ":"                                EOL \
                                             : "=r"(imm));
        #define JUMP(addr)      asm volatile("mov pc,%0" : : "r"(addr));
    #elif defined(__xtensa__)
        #define OP_ALIGN        4
        #define ASM_GUARD(code) while(dummy) { code; asm(""); }
        #define ASM_GET_IMM(ID) register size_t imm;                                            \
                                asm volatile("memw; j .LOAD_" ID                            EOL \
                                             ".align 4"                                     EOL \
                                             ".IMM_" ID ":"                                 EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + " ID ")"  EOL \
                                             ".LOAD_" ID ":"                                EOL \
                                             "l32r %0, .IMM_" ID                            EOL \
                                             : "=r"(imm));
        // TODO/OPTZ: achieve alignment without jump?
        #define ASM_ALIGN_NOP() asm volatile("j .eop_" STRINGIFY(__LINE__)                  EOL \
                                             ".balign " STRINGIFY(OP_ALIGN) ",0x00,16"      EOL \
                                             ".eop_" STRINGIFY(__LINE__) ":");
        #define JUMP(addr)      asm volatile("jx %0" : : "r"(addr));
    #else
        #error "Platform not supported"
    #endif
        
    #if OP_ALIGN <= 1
        #define IS_OP_ALIGNED(addr)         true
        #define ASM_ALIGN_ZERO()
        #define ASM_ALIGN_NOP()
    #else
        #ifndef IS_OP_ALIGNED
            #define IS_OP_ALIGNED(addr)     IS_ALIGNED(addr,OP_ALIGN)
        #endif
        #ifndef ASM_ALIGN_ZERO
            #define ASM_ALIGN_ZERO()        asm(".balign " STRINGIFY(OP_ALIGN) ",0x00,16")
        #endif
        #ifndef ASM_ALIGN_NOP
            #define ASM_ALIGN_NOP()         asm(".align " STRINGIFY(OP_ALIGN) ",,16")
        #endif
    #endif

    #ifndef DUMP
        #define DUMP 0
    #endif
    
    #if DUMP
        void dump(void* addr, size_t len) {
            uint8_t* p = (uint8_t*)addr;
            for (size_t i = 0; i<len; i++) {
                DBG_PRINTF(" %02x", p[i]);
            }
            DBG_PRINTF("\n");
        }
    #else
        #define dump(addr,len)
    #endif

    #define OP(op, code)            ASM_GUARD(                              \
                                      ASM_ALIGN_ZERO();                     \
                                      label_##op: {                         \
                                        { code; }                           \
                                        ASM_ALIGN_NOP();                    \
                                      } label_##op##_end:                   \
                                    )

    #define EMIT_OP(op)             { OpChunk* c = &gLabelTable[op_##op];   \
                                    ASSERT(c->addr); ASSERT(c->len > 0);    \
                                    char buff[c->len+8];                    \
                                    TEXT_READ(buff, c->addr, c->len);       \
                                    dump(buff, c->len);                     \
                                    TEXT_WRITE(vPC, buff, c->len);          \
                                    vPC = (void**)((char*)vPC + c->len); }

    #define EMIT_OP_IMM(op,imm)     { OpChunk* c = &gLabelTable[op_##op];   \
                                    ASSERT(c->addr); ASSERT(c->len > 0);    \
                                    char buff[c->len+8];                    \
                                    TEXT_READ(buff, c->addr, c->len);       \
                                    mempatch(buff, c->len, PLACEHOLDER + (int)op_##op, (size_t)(imm));     \
                                    dump(buff, c->len);                     \
                                    TEXT_WRITE(vPC, buff, c->len);          \
                                    vPC = (void**)((char*)vPC + c->len); }

    #define NEXT()                  not_implemented(); // Not implemented

    typedef struct OpChunk {
        void*   addr;
        int     len;
    } OpChunk;

    static OpChunk gLabelTable[Opcode_qty];

    static inline
    int TEXT_SECTION interp_run(void** prog)
    {
        size_t* volatile vSP = &gStack[STACK_SIZE-1];
        void** volatile vPC = prog;

        if (prog) {
            goto *prog;
        }

        #define STORE_OP_N(tgt,op,n)    { OpChunk* c = &gLabelTable[op_##tgt]; c->addr = &&label_##op; c->len = n;      \
                                          ASSERT(c->addr); ASSERT(c->len > 0);                                          \
                                          if (DUMP) {                                                                   \
                                            char buff[c->len+8]; TEXT_READ(buff, c->addr, c->len);                      \
                                            DBG_PRINTF("OP %s (%p, %d):", #tgt, c->addr, c->len); dump(buff, c->len);   \
                                          }                                                                             \
                                          ASSERT(IS_OP_ALIGNED(c->addr)); ASSERT(IS_OP_ALIGNED(c->len));                \
                                        }

        #define STORE_OP(op)            STORE_OP_N(op, op, (char*)&&label_##op##_end - (char*)&&label_##op)

        STORE_OP(push);
        STORE_OP(drop);
        STORE_OP(dup);
        STORE_OP(inc);
        STORE_OP(dec);
        STORE_OP(add);
        STORE_OP(sub);
        STORE_OP(jmp);
        STORE_OP(jnz);
        STORE_OP_N(halt, halt, 64);     // Need more because of jump

        #undef STORE_OP_N
        #undef STORE_OP

        volatile bool dummy = true;
        if (dummy) {
            return 1;
        }

        #include "opcodes_impl.h"

        for(;;) { dummy = true; }       // Should not reach here
    }

#else
    #error "Interpreter mode not defined"
#endif

static inline
void interp_init() {
    interp_run(NULL);
}
