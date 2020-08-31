typedef enum Opcode {
    op_push,
    op_drop,
    op_dup,
    op_inc,
    op_dec,
    op_add,
    op_sub,
    op_print,
    op_jmp,
    op_jnz,
    op_halt,

    Opcode_qty
} Opcode;
