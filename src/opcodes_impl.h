
OP(drop, {
    vSP++;
})

OP(push, {
    GET_IMM(0);
    *vSP-- = imm;
})

OP(dup, {
    vSP[0] = vSP[1];
    vSP--;
})

OP(add, {
    vSP[2] += vSP[1];
    vSP++;
})

OP(sub, {
    vSP[2] -= vSP[1];
    vSP++;
})

OP(inc, {
    GET_IMM(3);
    vSP[1] += imm;
})

OP(dec, {
    GET_IMM(4);
    vSP[1] -= imm;
})

OP(print, {
    DBG_PRINTF("Stack:");
    size_t* sp = vSP+1;
    while(sp < &gStack[STACK_SIZE]) {
        DBG_PRINTF(" %zu", *sp++);
    }
    DBG_PRINTF("\n");
    //usleep(10 * 1000);
})

OP(jmp, {
    GET_IMM(8);
    JUMP(imm);
})

OP(jnz, {
    GET_IMM(9);
    if (0 != *++vSP) JUMP(imm);
})

// Should be always the last one
OP(halt, {
    return 0;
})
