#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassemble_chunk (Chunk *c, const char *name) {
    printf ("== %s ==\n", name);

    for (int offset = 0; offset < c->count;) {
        offset = disassemble_instruction (c, offset);
    }
}

static int constant_instruction (const char *name, Chunk *c, 
                                int offset) {
    uint8_t c_idx = c->code[offset + 1];
    printf ("%-16s %4d '", name, c_idx);
    print_value (c->constants.values[c_idx]);
    printf ("'\n");
    return offset + 2;
}

static int simple_instruction (const char *name, int offset) {
    printf ("%s\n", name);
    return offset + 1;
}

static int byte_instruction (const char *name, Chunk *c, int offset) {
    uint8_t slot = c->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2; 
}

static int jmp_instruction (const char *name, int sign, 
                            Chunk *c, int offset) {
    uint16_t jmp = (uint16_t) (c->code[offset + 1] << 8);
    jmp |= c->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset,
            offset + 3 + sign * jmp);
    return offset + 3;

}

int disassemble_instruction (Chunk *c, int offset) {
    printf ("%04d ", offset);
    
    if (offset > 0 && c->lines[offset] == c->lines[offset - 1]) 
        printf ("   | ");
    else
        printf ("%4d ", c->lines[offset]);

    uint8_t instruction = c->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction ("OP_CONSTANT", c, offset);
        case OP_NIL:
            return simple_instruction ("OP_NIL", offset);
        case OP_TRUE:
            return simple_instruction ("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction ("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction ("OP_POP", offset); 
        case OP_DEFINE_GLOBAL:
            return constant_instruction ("OP_DEFINE_GLOBAL", c, offset);
        case OP_GET_GLOBAL:
            return constant_instruction ("OP_GET_GLOBAL", c, offset);
        case OP_SET_GLOBAL:
            return constant_instruction ("OP_SET_GLOBAL", c, offset);
        case OP_GET_LOCAL:
            return byte_instruction ("OP_GET_LOCAL", c, offset);
        case OP_SET_LOCAL:
            return byte_instruction ("OP_SET_LOCAL", c, offset);
        case OP_NEGATE:
            return simple_instruction ("OP_NEGATE", offset);
        case OP_EQUAL:
            return simple_instruction ("OP_EQUAL", offset);
        case OP_GREATER:
            return simple_instruction ("OP_GREATER", offset);
        case OP_LESS:
            return simple_instruction ("OP_LESS", offset);
        case OP_ADD:
            return simple_instruction ("OP_ADD", offset);
        case OP_SUBTRACT:
            return simple_instruction ("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simple_instruction ("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simple_instruction ("OP_DIVIDE", offset);
        case OP_NOT:
            return simple_instruction ("OP_NOT", offset);
        case OP_PRINT:
            return simple_instruction ("OP_PRINT", offset);
        case OP_JMP:
            return jmp_instruction ("OP_JMP", 1, c, offset);
        case OP_JMP_IF_FALSE:
            return jmp_instruction ("OP_JMP_IF_FALSE", 1, c, offset);
        case OP_LOOP:
            return jmp_instruction ("OP_LOOP", -1, c, offset);
        case OP_RETURN:
            return simple_instruction ("OP_RETURN", offset);
        default:
            printf ("Unknown opcode: %d\n", instruction);
            return offset + 1;
    }
}