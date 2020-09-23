#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "interp_utils.h"
#define OP_ENUM
#include "opcodes.h"
#undef OP_ENUM

#define STACK_SIZE 256
static size_t gStack[STACK_SIZE] = { (size_t)-1, };

// Syntactic Helpers

#define LABEL(lbl)                  void** lbl = vPC;           // Store code position to a label
#define PUSH(imm)                   EMIT_OP_IMM(push,imm);      // Push constant to stack
#define DUP()                       EMIT_OP(dup);               // Duplicate stack top
#define DROP()                      EMIT_OP(drop);              // Drop top value from stack
#define JMP(imm)                    EMIT_OP_IMM(jmp,imm);       // Jump
#define JNZ(imm)                    EMIT_OP_IMM(jnz,imm);       // Jump if Not Zero
#define JNZP(imm)                   EMIT_OP_IMM(jnzp,imm);      // Jump if Not Zero + Pop
#define INCN(imm)                   EMIT_OP_IMM(incN,imm);      // Increase by a constant
#define DECN(imm)                   EMIT_OP_IMM(decN,imm);      // Decrease by a constant
#define INC()                       EMIT_OP(inc);               // Increase by 1
#define DEC()                       EMIT_OP(dec);               // Decrease by 1
#define ADD(imm)                    EMIT_OP(add);               // Add 2 top stack values
#define SUB(imm)                    EMIT_OP(sub);               // Sub 2 top stack values
#define MUL(imm)                    EMIT_OP(mul);               // Mul 2 top stack values
#define DIV(imm)                    EMIT_OP(div);               // Div 2 top stack values
#define HALT()                      EMIT_OP(halt);              // Stop VM

#if defined(USE_INLINE) && defined(__wasm__)
    #warning "Cannot INLINE code on WASM target, using DTC"
    #undef USE_INLINE
    #define USE_DTC
#endif

#if !defined(USE_INLINE)

    #define EMIT_OP_IMM(op,imm)     { EMIT_OP(op); *vPC++ = (void*)(imm); }

    #define GET_IMM(op)             register size_t imm = (size_t)*vPC++;
    #define JUMP(addr)              { vPC = (void**)(addr); NEXT(); }

#endif


#if defined(USE_DTC) || defined(USE_TTC)

    #if defined(USE_DTC)            // Direct Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         ASSERT(gLabelTable[op_##op]); *vPC++ = gLabelTable[op_##op]
        #define NEXT()              goto **(vPC++)
    #elif defined(USE_TTC)          // Indirect (Token) Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         ASSERT(gLabelTable[op_##op]); *vPC++ = (void*)op_##op
        #define NEXT()              goto *gLabelTable[(size_t)*vPC++]
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

        #define OP_STORE(label) gLabelTable[op_##label] = &&label_##label
        #include "opcodes.h"
        #undef OP_STORE
        return 0;

        #include "opcodes.h"
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
                #include "opcodes.h"
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

    #include "opcodes.h"

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

    #include "opcodes.h"

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

    #if UINTPTR_MAX == 0xFFFFFFFF
        #define PLACEHOLDER     0xdeadbeef
        #define ASM_SIZE_T      ".long "
    #elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
        #define PLACEHOLDER     0xdeadbeefdeadbeef
        #define ASM_SIZE_T      ".quad "
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
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("mov $(" STRINGIFY(PLACEHOLDER) " + %c[id]), %0"   \
                                             : "=r"(imm) : [id]"i"(ID));
        #define JUMP(addr)      asm volatile("jmp *%0" : : "r"(addr));
    #elif defined(__aarch64__)
        #define OP_ALIGN        4
        #define HALT_SIZE       32
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("ldr %0, .IMM_" STRINGIFY(ID)                  EOL \
                                             "b   .CONT_" STRINGIFY(ID)                     EOL \
                                             ".IMM_" STRINGIFY(ID) ":"                      EOL \
                                             ".quad (" STRINGIFY(PLACEHOLDER) " + %c[id])"  EOL \
                                             ".CONT_" STRINGIFY(ID) ":"                     EOL \
                                             : "=r"(imm) : [id]"i"(ID) : "x1");
        #define JUMP(addr)      asm volatile("br %0" : : "r"(addr));
    #elif defined(__arm__)
        #define OP_ALIGN        4
        #define HALT_SIZE       32
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("ldr %0, [pc, #0]"                             EOL \
                                             "b   .CONT_" STRINGIFY(ID)                     EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + %c[id])"  EOL \
                                             ".CONT_" STRINGIFY(ID) ":"                     EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        #define JUMP(addr)      asm volatile("mov pc,%0" : : "r"(addr));
    #elif defined(__mips__)
        #define OP_ALIGN        4
        #define HALT_SIZE       128
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("move $6, $ra"                                 EOL \
                                             "bal GetIP_" STRINGIFY(ID)                     EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + %c[id])"  EOL \
                                             "GetIP_" STRINGIFY(ID) ":"                     EOL \
                                             "move $7, $ra"                                 EOL \
                                             "move $ra, $6"                                 EOL \
                                             "lw %0, 0($7)"                                 EOL \
                                             : "=r"(imm) : [id]"i"(ID) : "$6", "$7");
        #define ASM_ALIGN_ZERO() // always aligned on mips
        #define ASM_ALIGN_NOP()  // always aligned on mips
        #define JUMP(addr)      asm volatile("jr %0" : : "r"(addr));
    #elif defined(__riscv)
        #define OP_ALIGN        4
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("auipc x10, 0"                                 EOL \
                                             "lw %0, 8(x10)"                                EOL \
                                             "j   .CONT_" STRINGIFY(ID)                     EOL \
                                             ASM_SIZE_T " (" STRINGIFY(PLACEHOLDER) " + %c[id])" EOL \
                                             ".CONT_" STRINGIFY(ID) ":"                     EOL \
                                             : "=r"(imm) : [id]"i"(ID) : "x10");
        #define ASM_ALIGN_ZERO()        asm(".align " STRINGIFY(OP_ALIGN))
        #define ASM_ALIGN_NOP()         asm(".align " STRINGIFY(OP_ALIGN))
        #define JUMP(addr)      asm volatile("c.jr %0" : : "r"(addr));
    #elif defined(__xtensa__)
        #define OP_ALIGN        4
        #define ASM_GUARD(code) while(dummy) { code; asm(""); }
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("memw; j .LOAD_" STRINGIFY(ID)                 EOL \
                                             ".align 4"                                     EOL \
                                             ".IMM_" STRINGIFY(ID) ":"                      EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + %c[id])"  EOL \
                                             ".LOAD_" STRINGIFY(ID) ":"                     EOL \
                                             "l32r %0, .IMM_" STRINGIFY(ID)                 EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        // TODO/OPTZ: achieve alignment without jump?
        #define ASM_ALIGN_NOP() asm volatile("j .eop_" STRINGIFY(__LINE__)                  EOL \
                                             ".balign " STRINGIFY(OP_ALIGN) ",0x00,16"      EOL \
                                             ".eop_" STRINGIFY(__LINE__) ":");
        #define JUMP(addr)      asm volatile("jx %0" : : "r"(addr));
    #else
        #error "Platform not supported"
    #endif
        
    #ifndef ASM_GUARD
        #define ASM_GUARD(code)             asm(""); code; asm("");
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
        
    #ifndef HALT_SIZE
        #define HALT_SIZE 64
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

        #define OP_ADDR(op)             (&&label_##op)
        #define OP_SIZE(op)             ((char*)&&label_##op##_end - (char*)&&label_##op)

        #define OP_STORE_N(tgt,op,n)    { OpChunk* c = &gLabelTable[op_##tgt]; c->addr = OP_ADDR(op); c->len = (n);     \
                                          ASSERT(c->addr); ASSERT(c->len > 0);                                          \
                                          if (DUMP) {                                                                   \
                                            char buff[c->len+8]; TEXT_READ(buff, c->addr, c->len);                      \
                                            DBG_PRINTF("OP %s (%p, %d):", #tgt, c->addr, c->len); dump(buff, c->len);   \
                                          }                                                                             \
                                          ASSERT(IS_OP_ALIGNED(c->addr)); ASSERT(IS_OP_ALIGNED(c->len));                \
                                        }

        #define OP_STORE(op)            if (op_##op == op_halt) {                                                       \
                                          OP_STORE_N(op, op, HALT_SIZE);     /* Need more because of jump */            \
                                        } else {                                                                        \
                                          OP_STORE_N(op, op, OP_SIZE(op));                                              \
                                        }

        #include "opcodes.h"

        #undef OP_STORE_N
        #undef OP_STORE

        #undef OP_ADDR
        #undef OP_SIZE

        volatile bool dummy = true;
        if (dummy) {
            return 1;
        }

        #include "opcodes.h"

        for(;;) { dummy = true; }       // Should not reach here
    }

#else
    #error "Interpreter mode not defined"
#endif

static inline
void interp_init() {
    interp_run(NULL);
}
