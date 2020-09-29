#if defined(OP_ENUM)
    #define OP_IMPL(op,body)        op_##op,
    typedef enum Opcode {
#elif defined(OP_STORE)
    #define OP_IMPL(op,body)        OP_STORE(op);
#elif defined(OP)
    #define OP_IMPL(op,body)        OP(op,body)
    #define OP_IMPL_IMM(op,body)    OP(op, { GET_IMM(op_##op); body })
#else
    #error "Invalid opcodes.h usage"
#endif

#if !defined(OP_IMPL_IMM)
    #define OP_IMPL_IMM             OP_IMPL
#endif

OP_IMPL(dummy1, { asm("nop; #1"); })
OP_IMPL(dummy2, { asm("nop; #2"); })
OP_IMPL(dummy3, { asm("nop; #3"); })
OP_IMPL(dummy4, { asm("nop; #4"); })
OP_IMPL(dummy5, { asm("nop; #5"); })

OP_IMPL(drop, {
    vSP++;
})

OP_IMPL_IMM(push, {
    *vSP-- = imm;
})

OP_IMPL(dup, {
    vSP[0] = vSP[1];
    vSP--;
})

OP_IMPL(add, {
    vSP[2] += vSP[1];
    vSP++;
})

OP_IMPL(sub, {
    vSP[2] -= vSP[1];
    vSP++;
})

OP_IMPL(mul, {
    vSP[2] *= vSP[1];
    vSP++;
})

OP_IMPL(div, {
    vSP[2] /= vSP[1];
    vSP++;
})

OP_IMPL_IMM(incN, {
    vSP[1] += imm;
})

OP_IMPL_IMM(decN, {
    vSP[1] -= imm;
})

OP_IMPL(inc, {
    vSP[1] += 1;
})

OP_IMPL(dec, {
    vSP[1] -= 1;
})

OP_IMPL(print, {
    DBG_PRINTF("Stack:");
    size_t* sp = vSP+1;
    while(sp < &gStack[STACK_SIZE]) {
        DBG_PRINTF(" %zu", *sp++);
    }
    DBG_PRINTF("\n");
    //usleep(10 * 1000);
})

OP_IMPL_IMM(jmp, {
    JUMP(imm);
})

OP_IMPL_IMM(jnzp, {
    if (*++vSP) JUMP(imm);
})

OP_IMPL_IMM(jnz, {
    if (vSP[1]) JUMP(imm);
})

// Should be always the last one
OP_IMPL(halt, {
    FINISH();
})

#if defined(OP_ENUM)
    Opcode_qty
} Opcode;
#endif

#undef OP_IMPL
#undef OP_IMPL_IMM
