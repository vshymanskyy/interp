#ifndef INTERP_H
#define INTERP_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// MSVC handling
#if defined(_MSC_VER) && !defined(__clang__)
    #if defined(USE_INLINE) || defined(USE_DTC) || defined (USE_TTC)
        #pragma message("MSVC compiler only supports SWITCH, CALLS, TAIL_CALLS => using SWITCH")
        #undef USE_INLINE
        #undef USE_DTC
        #undef USE_TTC
        #define USE_SWITCH
    #endif
#endif

// WebAssembly handling
#if defined(USE_INLINE) && defined(__wasm__)
    #warning "Cannot INLINE code on WASM target => using DTC"
    #undef USE_INLINE
    #define USE_DTC
#endif

#include "interp_utils.h"
#define OP_ENUM
#include "opcodes.h"
#undef OP_ENUM

#define STACK_SIZE 256
static size_t gStack[STACK_SIZE] = { (size_t)-1, };

// Native helpers, called by address (passed as an immediate), so that the INLINE mode
// can copy the calling code
static
void interp_print(size_t* sp)
{
    DBG_PRINTF("Stack:");
    for (sp++; sp < &gStack[STACK_SIZE]; sp++) {
        DBG_PRINTF(" %lu", (unsigned long)*sp);
    }
    DBG_PRINTF("\n");
}

static
size_t interp_div(size_t a, size_t b)
{
    return a / b;       // May be a runtime library call (no hardware divide)
}

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
#define ADD()                       EMIT_OP(add);               // Add 2 top stack values
#define SUB()                       EMIT_OP(sub);               // Sub 2 top stack values
#define MUL()                       EMIT_OP(mul);               // Mul 2 top stack values
#define DIV()                       EMIT_OP_IMM(div,interp_div);    // Div 2 top stack values
#define PRINT()                     EMIT_OP_IMM(print,interp_print);// Print the stack
#define HALT()                      EMIT_OP(halt);              // Stop VM

// Check that the emitted code fits into the buffer from malloc_exec()
#define ASSERT_EMIT(len)            ASSERT(!gExecEnd || (char*)vPC + (len) <= gExecEnd)


#if !defined(USE_INLINE)
    #define EMIT_PTR(val)           do { ASSERT_EMIT(sizeof(void*)); *vPC++ = (void*)(val); } while (0)
    #define EMIT_OP_IMM(op,imm)     do { EMIT_OP(op); EMIT_PTR((size_t)(imm)); } while (0)

    #define GET_IMM(op)             register size_t imm = (size_t)*vPC++;
    #define JUMP(addr)              { vPC = (void**)(addr); NEXT(); }
#endif


#if defined(USE_DTC) || defined(USE_TTC)

    #if defined(USE_DTC)            // Direct Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         do { ASSERT(gLabelTable[op_##op]); EMIT_PTR(gLabelTable[op_##op]); } while (0)
        #define NEXT()              goto **(vPC++)
    #elif defined(USE_TTC)          // Indirect (Token) Threaded Code
        #define OP(op, code)        label_##op: { code; } NEXT();
        #define EMIT_OP(op)         do { ASSERT(gLabelTable[op_##op]); EMIT_PTR((size_t)op_##op); } while (0)
        #define NEXT()              goto *gLabelTable[(size_t)*vPC++]
    #endif
    #define FINISH()                return 0;

    static void* gLabelTable[Opcode_qty];

    // Must not be inlined: the label table has to point into the same copy that runs the code
    static NOINLINE
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
    #define EMIT_OP(op)         EMIT_PTR((size_t)op_##op)
    #define NEXT()              continue;
    #define FINISH()            return 0;

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

    // MUSTTAIL guarantees the tail call (where supported), so the stack does not grow even at -O0
    #define OP(op, code)        static int opf_##op(OpSig) { { code; } NEXT(); }
    #define EMIT_OP(op)         EMIT_PTR(opf_##op)
    #define NEXT()              MUSTTAIL return ((OpFunc)(* vPC))(vPC + 1, vSP);
    #define FINISH()            return 0;

    typedef int (*OpFunc)(OpSig);

    #include "opcodes.h"

    static inline
    int interp_run(void** prog)
    {
        if (!prog) return 0;

        return ((OpFunc)(* prog))(prog + 1, &gStack[STACK_SIZE-1]);
    }

#elif defined(USE_CALLS)

    #define OP(op, code)        static void* opf_##op(void) { { code; } NEXT(); }
    #define EMIT_OP(op)         EMIT_PTR(opf_##op)
    #define NEXT()              return *vPC++;
    #define FINISH()            return (void*)fireEscape;

    static void* fireEscape(void) {
        return (void*)fireEscape;
    }

    typedef void* (* OpFunc)(void);

    static void** vPC;
    static size_t* vSP;

    #include "opcodes.h"

    static inline
    int interp_run(void** prog)
    {
        if (!prog) return 0;

        vPC = prog;
        vSP = &gStack[STACK_SIZE-1];

        OpFunc pfn = (OpFunc)*vPC++;

        #define DISPATCH1       pfn = (OpFunc)(pfn());
        #define DISPATCH2       DISPATCH1;  DISPATCH1;
        #define DISPATCH4       DISPATCH2;  DISPATCH2;
        #define DISPATCH8       DISPATCH4;  DISPATCH4;

        // Unrolled: once halted, the remaining calls go to fireEscape, which is harmless
        while (pfn != fireEscape) {
            DISPATCH8;
        }
        return 0;
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

    // Note: use numeric local labels in the asm below. Named labels become duplicates
    // if the compiler emits the code twice, and real symbols on Mach-O (Apple),
    // which breaks the literal fixups and CFI generation
    #if defined(__x86_64__) || defined(__i386__)
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("mov $(" STRINGIFY(PLACEHOLDER) " + %c[id]), %0"   \
                                             : "=r"(imm) : [id]"i"(ID));
        #define JUMP(addr)      asm volatile("jmp *%0" : : "r"(addr));
    #elif defined(__aarch64__)
        #define OP_ALIGN        4
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("ldr %0, 1f"                                   EOL \
                                             "b   2f"                                       EOL \
                                             "1: .quad (" STRINGIFY(PLACEHOLDER) " + %c[id])" EOL \
                                             "2:"                                           EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        #define JUMP(addr)      asm volatile("br %0" : : "r"(addr));
    #elif defined(__arm__)
        #define OP_ALIGN        4
        // Load the literal by label: the PC offset differs between ARM and Thumb modes
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("ldr %0, 1f"                                   EOL \
                                             "b   2f"                                       EOL \
                                             ".balign 4"                                    EOL \
                                             "1: .long (" STRINGIFY(PLACEHOLDER) " + %c[id])" EOL \
                                             "2:"                                           EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        #define JUMP(addr)      asm volatile("mov pc,%0" : : "r"(addr));
    #elif defined(__mips__)
        #define OP_ALIGN        4
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("move $6, $ra"                                 EOL \
                                             "bal 1f"                                       EOL \
                                             ".long (" STRINGIFY(PLACEHOLDER) " + %c[id])"  EOL \
                                             "1:"                                           EOL \
                                             "move $7, $ra"                                 EOL \
                                             "move $ra, $6"                                 EOL \
                                             "lw %0, 0($7)"                                 EOL \
                                             : "=r"(imm) : [id]"i"(ID) : "$6", "$7");
        #define ASM_ALIGN_ZERO() // always aligned on mips
        #define ASM_ALIGN_NOP()  // always aligned on mips
        #define JUMP(addr)      asm volatile("jr %0" : : "r"(addr));
    #elif defined(__riscv)
        #define OP_ALIGN        4
        #if __riscv_xlen == 64
            #define ASM_LOAD_SIZE_T     "ld "
        #else
            #define ASM_LOAD_SIZE_T     "lw "
        #endif
        // Load the literal by label, so the offset holds with compressed instructions.
        // norelax keeps the linker from turning it into a (non-relocatable) gp-relative load
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile(".option push"                                 EOL \
                                             ".option norelax"                              EOL \
                                             ASM_LOAD_SIZE_T "%0, 1f"                       EOL \
                                             "j   2f"                                       EOL \
                                             ".balign 8"                                    EOL \
                                             "1: " ASM_SIZE_T " (" STRINGIFY(PLACEHOLDER) " + %c[id])" EOL \
                                             "2:"                                           EOL \
                                             ".option pop"                                  EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        #define ASM_ALIGN_ZERO()        asm(".align " STRINGIFY(OP_ALIGN))
        #define ASM_ALIGN_NOP()         asm(".align " STRINGIFY(OP_ALIGN))
        #define JUMP(addr)      asm volatile("c.jr %0" : : "r"(addr));
    #elif defined(__xtensa__)
        #define OP_ALIGN        4
        #define ASM_GUARD(code) while(dummy) { code; asm(""); }
        #define GET_IMM(ID)     register size_t imm;                                            \
                                asm volatile("memw; j 2f"                                   EOL \
                                             ".align 4"                                     EOL \
                                             "1: .long (" STRINGIFY(PLACEHOLDER) " + %c[id])" EOL \
                                             "2: l32r %0, 1b"                               EOL \
                                             : "=r"(imm) : [id]"i"(ID));
        // TODO/OPTZ: achieve alignment without jump?
        #define ASM_ALIGN_NOP() asm volatile("j 1f"                                         EOL \
                                             ".balign " STRINGIFY(OP_ALIGN) ",0x00"         EOL \
                                             "1:");
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
            #define ASM_ALIGN_ZERO()        asm(".balign " STRINGIFY(OP_ALIGN) ",0x00")
        #endif
        #ifndef ASM_ALIGN_NOP
            #define ASM_ALIGN_NOP()         asm(".balign " STRINGIFY(OP_ALIGN))
        #endif
    #endif

    #ifndef DUMP
        #define DUMP 0
    #endif

    #if DUMP
        static void dump(void* addr, size_t len) {
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

    // Note: the ops are copied as is, so they must not contain relative references to
    // anything outside of the op (calls, globals, literal pools). Pass such addresses as an immediate
    #define EMIT_OP(op)             do { OpChunk* c = &gLabelTable[op_##op];    \
                                    ASSERT(c->addr); ASSERT(c->len > 0);        \
                                    ASSERT_EMIT(c->len);                        \
                                    char buff[c->len+8];                        \
                                    TEXT_READ(buff, c->addr, c->len);           \
                                    dump(buff, c->len);                         \
                                    TEXT_WRITE(vPC, buff, c->len);              \
                                    vPC = (void**)((char*)vPC + c->len); } while (0)

    #define EMIT_OP_IMM(op,imm)     do { OpChunk* c = &gLabelTable[op_##op];    \
                                    ASSERT(c->addr); ASSERT(c->len > 0);        \
                                    ASSERT_EMIT(c->len);                        \
                                    char buff[c->len+8];                        \
                                    TEXT_READ(buff, c->addr, c->len);           \
                                    mempatch(buff, c->len, PLACEHOLDER + (int)op_##op, (size_t)(imm));     \
                                    dump(buff, c->len);                         \
                                    TEXT_WRITE(vPC, buff, c->len);              \
                                    vPC = (void**)((char*)vPC + c->len); } while (0)

    #define NEXT()                  not_implemented(); // Not implemented

    // A plain return may compile to a (relative) jump to the shared function epilogue,
    // which breaks once the code is copied. Instead, jump back to interp_run by absolute address
    #define FINISH()                { GET_IMM(op_halt); JUMP(imm); }
    #undef  HALT
    #define HALT()                  EMIT_OP_IMM(halt, gExitAddr);

    typedef struct OpChunk {
        void*   addr;
        int     len;
    } OpChunk;

    static OpChunk gLabelTable[Opcode_qty];
    static void* gExitAddr;
    static void* volatile gOpsBegin;    // only to keep the ops_begin anchor as a separate block

    // Must not be inlined: the label table has to point into the same copy that runs the code
    static NOINLINE
    int TEXT_SECTION interp_run(void** prog)
    {
        size_t* volatile vSP = &gStack[STACK_SIZE-1];

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

        #define OP_STORE(op)            OP_STORE_N(op, op, OP_SIZE(op))

        #include "opcodes.h"

        #undef OP_STORE_N
        #undef OP_STORE

        #undef OP_ADDR
        #undef OP_SIZE

        gExitAddr = &&label_exit;
        gOpsBegin = &&label_ops_begin_end;
        (void)&&label_ops_begin;

        volatile bool dummy = true;
        if (dummy) {
            return 1;
        }

        // The compiler may place the block that precedes the first op anywhere, losing its
        // alignment. Start with an unused op, so every real op follows an aligned one
        OP(ops_begin, {})

        #include "opcodes.h"

    label_exit:                         // halt jumps here
        return 0;
    }

#else
    #error "Interpreter mode not defined"
#endif

static inline
void interp_init() {
    interp_run(NULL);
}

#endif // INTERP_H
