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

OP_IMPL(dummy1, { ASM_NOP(1); })
OP_IMPL(dummy2, { ASM_NOP(2); })
OP_IMPL(dummy3, { ASM_NOP(3); })
OP_IMPL(dummy4, { ASM_NOP(4); })
OP_IMPL(dummy5, { ASM_NOP(5); })

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

// Calls interp_div by address, as the division may need a runtime library call
OP_IMPL_IMM(div, {
    vSP[2] = ((size_t (*)(size_t, size_t))imm)(vSP[2], vSP[1]);
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

// Calls interp_print by address
OP_IMPL_IMM(print, {
    ((void (*)(size_t*))imm)(vSP);
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

// Indirect control flow

OP_IMPL(swap, {
    size_t tmp = vSP[1];
    vSP[1] = vSP[2];
    vSP[2] = tmp;
})

OP_IMPL(jmpi, {                             // [addr] -> []
    size_t addr = *++vSP;
    JUMP(addr);
})

// Comparison (unsigned), result is 1 or 0

OP_IMPL(eq, {
    vSP[2] = (vSP[2] == vSP[1]);
    vSP++;
})

OP_IMPL(ne, {
    vSP[2] = (vSP[2] != vSP[1]);
    vSP++;
})

OP_IMPL(lt, {
    vSP[2] = (vSP[2] < vSP[1]);
    vSP++;
})

OP_IMPL(gt, {
    vSP[2] = (vSP[2] > vSP[1]);
    vSP++;
})

// Bitwise

OP_IMPL(band, {
    vSP[2] &= vSP[1];
    vSP++;
})

OP_IMPL(bor, {
    vSP[2] |= vSP[1];
    vSP++;
})

OP_IMPL(bxor, {
    vSP[2] ^= vSP[1];
    vSP++;
})

OP_IMPL(bnot, {
    vSP[1] = ~vSP[1];
})

OP_IMPL(shl, {
    vSP[2] <<= (vSP[1] & (sizeof(size_t)*8 - 1));
    vSP++;
})

OP_IMPL(shr, {
    vSP[2] >>= (vSP[1] & (sizeof(size_t)*8 - 1));
    vSP++;
})

// Memory access. The immediate is the memory base address (gMemory),
// the address on the stack is an offset into it (unaligned word access is allowed).
// Bounds are checked in all modes except INLINE

OP_IMPL_IMM(load, {                         // [addr] -> [value]
    OP_ASSERT(vSP[1] + sizeof(size_t) <= MEMORY_SIZE);
    vSP[1] = LOAD_UNALIGNED(imm + vSP[1]);
})

OP_IMPL_IMM(store, {                        // [value, addr] -> []
    OP_ASSERT(vSP[1] + sizeof(size_t) <= MEMORY_SIZE);
    STORE_UNALIGNED(imm + vSP[1], vSP[2]);
    vSP += 2;
})

OP_IMPL_IMM(load8, {                        // [addr] -> [value]
    OP_ASSERT(vSP[1] < MEMORY_SIZE);
    vSP[1] = *(uint8_t*)(imm + vSP[1]);
})

OP_IMPL_IMM(store8, {                       // [value, addr] -> []
    OP_ASSERT(vSP[1] < MEMORY_SIZE);
    *(uint8_t*)(imm + vSP[1]) = (uint8_t)vSP[2];
    vSP += 2;
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
