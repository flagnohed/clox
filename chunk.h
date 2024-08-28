#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"


typedef enum {
    OP_RETURN,
}   OpCode;

typedef struct {
    int capacity;
    int count;
    uint8_t *code;
}   Chunk;


void init_chunk (Chunk *c);
void free_chunk (Chunk *c);
void write_chunk (Chunk *c, uint8_t byte);



#endif

