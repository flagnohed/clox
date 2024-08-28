#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void init_chunk (Chunk *c) {
    c->capacity = 0;
    c->count = 0;
    c->code = NULL;
}

void free_chunk (Chunk *c) {
    FREE_ARRAY(uint8_t, c->code, c->capacity);
    // zero that fucker out
    init_chunk (c);
}

void write_chunk (Chunk *c, uint8_t byte) {
    if (c->count == c->capacity) {
        int old_capacity = c->capacity;
        c->capacity = GROW_CAPACITY(old_capacity);
        c->code = GROW_ARRAY(uint8_t, c->code, 
            old_capacity, c->capacity);
    }
    c->code[c->count] = byte;
    c->count++;
}