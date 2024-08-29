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
        case OP_NEGATE:
            return simple_instruction ("OP_NEGATE", offset);
        case OP_RETURN:
            return simple_instruction ("OP_RETURN", offset);
        default:
            printf ("Unknown opcode: %d\n", instruction);
            return offset + 1;
    }
}