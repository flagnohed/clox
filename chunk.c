#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void init_chunk (Chunk *c) {
    c->capacity = 0;
    c->count = 0;
    c->lines = NULL;
    c->code = NULL;
    init_value_array (&c->constants);
}

void free_chunk (Chunk *c) {
    FREE_ARRAY(uint8_t, c->code, c->capacity);
    FREE_ARRAY(int, c->lines, c->capacity);
    free_value_array (&c->constants);
    // zero that fucker out
    init_chunk (c);
}

void write_chunk (Chunk *c, uint8_t byte, int line) {
    if (c->count == c->capacity) {
        int old_capacity = c->capacity;
        c->capacity = GROW_CAPACITY(old_capacity);
        c->code = GROW_ARRAY(uint8_t, c->code, 
            old_capacity, c->capacity);
        c->lines = GROW_ARRAY(int, c->lines, old_capacity, 
            c->capacity);
    }
    c->code[c->count] = byte;
    c->lines[c->count] = line;
    c->count++;
}

int add_constant (Chunk *c, Value val) {
    write_value_array (&c->constants, val);
    return c->constants.count - 1;
}